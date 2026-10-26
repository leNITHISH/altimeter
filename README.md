# 8051 barometric altimeter

An AT89S52 reads a BMP280 pressure sensor over bit-banged I2C, turns the pressure into altitude, and shows it on a 16x2 character LCD. Tap a button to zero it, lift the board 1 m, and it reads +1.0 m. The altitude is corrected for the measured air temperature, smoothed by a 2-state Kalman filter that also gives vertical speed, and a second button lets you enter today's sea-level pressure (QNH) so absolute mode shows real height above sea level.

This is an MPMC (Microprocessors and Microcontrollers) course project. Everything is written in C for SDCC on Arch Linux and flashed with a USBasp-compatible programmer through avrdude. No Keil, no Windows.

```
Alt:     1.0 m R      <- temperature-corrected altitude, R = relative (zeroed), A = absolute
T 32.8C P1013.9       <- line 2: one of four views (T/P, ISA comparison, vertical speed, QNH)
```

The slides for the final review are in [`deck/altimeter-final-review.pdf`](deck/altimeter-final-review.pdf).

## Contents

- [Status](#status)
- [Hardware](#hardware)
- [Wiring](#wiring)
- [Toolchain setup](#toolchain-setup)
- [Building and flashing](#building-and-flashing)
- [Using the altimeter](#using-the-altimeter)
- [How it works](#how-it-works)
- [Memory budget](#memory-budget)
- [Source tour](#source-tour)
- [Host-side tools](#host-side-tools)
- [Accuracy and limitations](#accuracy-and-limitations)
- [Barometric vs GPS altitude](#barometric-vs-gps-altitude)
- [Troubleshooting](#troubleshooting)
- [Development history](#development-history)

## Status

Tested on the real board:

| Feature | Result |
|---|---|
| LED heartbeat on P2.0 | works |
| LCD, 4-bit mode, 10k contrast pot | works, readable straight on |
| I2C scan | sensor found at 0x76 |
| Chip ID | 0x58, a BMP280 (sold as a BME280, which is common) |
| Compensated temperature and pressure | 32.8 °C, 1013.9 hPa in a warm room |
| Onboard button (P3.3) | tap zeroes, hold 2 s returns to absolute mode |
| External button (P3.2) | tap cycles the four line-2 views, hold 1 s opens the QNH editor |
| Lift test | about +1.0 m when raised 1 m after zeroing, back to about 0 when lowered; the ISA view reads slightly lower |
| Vertical speed | positive going up, about 0 at rest, negative going down |
| QNH editor | saves valid values and switches to absolute mode; 0800.0 is rejected with `Only 870-1085!` |

Not tested:

| Feature | Why |
|---|---|
| UART debug output | No USB-to-RS232 adapter is available. The "2-in-1" programmer is only a USBasp (USB ID 16c0:05dc) and has no serial port. The prints are compiled in only with `EXTRA=-DUART_DEBUG`. |
| Kalman filter on logged data | Without a serial adapter no real data could be logged, so the tuning comes from a simulation (`tools/kalman_sim.py`) and was checked on the board by eye. |
| Humidity | The fitted sensor is a BMP280, which has no humidity sensor. The firmware doesn't read humidity on a real BME280 either. |

Current build: **8118 of 8192 bytes of flash** (74 bytes left), stack starts at 0xC1 with 63 bytes free. A known-good build from before the final-review features is tagged `demo-v1`.

## Hardware

| Part | Details |
|---|---|
| Board | MyTechnoCare "My 8051" development board, MY010V2.1 (borrowed, so nothing in this project risks damaging it) |
| MCU | Atmel AT89S52: 8 KB flash, 256 bytes internal RAM, three timers, no EEPROM, no external RAM fitted |
| Clock | 11.0592 MHz crystal, 12 clocks per machine cycle, so one machine cycle is 1.085 µs |
| Sensor | 5 V BMP280/BME280 breakout with an onboard 3.3 V regulator and I2C level shifter |
| Display | 16x2 HD44780-compatible character LCD with backlight, contrast set by a 10k potentiometer |
| Buttons | onboard push button on P3.3 (jumper JP2), plus one external push button on P3.2 |
| Programmer | MyTechnoCare 2-in-1 USB ISP programmer, a USBasp clone with old firmware |
| Other onboard parts used | green LED on P2.0 (jumper JP3), MAX232 on P3.0/P3.1 |

## Wiring

### Pin map

| Signal | 8051 pin | Notes |
|---|---|---|
| I2C SCL | P1.0 | bit-banged |
| I2C SDA | P1.1 | bit-banged |
| LCD RS | P2.1 | |
| LCD E | P2.2 | |
| LCD D4..D7 | P2.4..P2.7 | 4-bit mode |
| Status LED | P2.0 | onboard, toggles on every sensor reading |
| Zero button | P3.3 | onboard, active low |
| View / QNH button | P3.2 | external push button between the pin and GND |
| UART TX / RX | P3.1 / P3.0 | onboard MAX232 to the DB9 connector, 9600 8N1, debug build only |

P2.3 is free. It drove the LCD contrast by PWM until the pot was fitted (see [Development history](#development-history)).

Port 0 is not used at all. On this board it's open drain with no pull-up resistors fitted, so it can't drive anything high.

### LCD (16-pin header)

| LCD pin | Name | Connect to |
|---|---|---|
| 1 | VSS | GND |
| 2 | VDD | 5 V |
| 3 | V0 | wiper of a 10k pot whose ends go to 5 V and GND |
| 4 | RS | P2.1 |
| 5 | RW | GND |
| 6 | E | P2.2 |
| 7–10 | D0–D3 | not connected |
| 11 | D4 | P2.4 |
| 12 | D5 | P2.5 |
| 13 | D6 | P2.6 |
| 14 | D7 | P2.7 |
| 15 | A (backlight +) | 5 V |
| 16 | K (backlight −) | GND |

RW must be tied to GND. If it floats, the LCD can sit in read mode, ignore every write, and drive D4–D7 against the 8051's pins. Because RW is grounded, the firmware never reads the busy flag and uses fixed delays instead.

Turn the pot until the characters are dark and the background is clear. V0 too close to GND makes every cell grey; too high and the screen goes blank.

The backlight goes straight to 5 V here, which is fine because the module has its own current-limiting resistor. If your module gets hot, add about 100 Ω in series with pin 15.

### Sensor breakout

| Breakout pin | Connect to |
|---|---|
| VCC / VIN | 5 V |
| GND | GND |
| SCL | P1.0 |
| SDA | P1.1 |
| SDO (if present) | GND for address 0x76, or 5 V/3.3 V for 0x77 |
| CSB (if present) | leave unconnected or high (selects I2C) |

The firmware tries 0x76 first, then 0x77, so either SDO setting works.

### External button

One leg to P3.2, the other to GND. No resistor: the port's internal pull-up holds the pin high, and pressing the button pulls it low, exactly like the onboard button. The firmware only ever reads this pin and never writes a 0 to it, so a wrong connection can't cause a short through the port.

### Why bit-banged I2C is safe on Port 1

Port 1 is quasi-bidirectional: writing a 1 turns on only a weak internal pull-up, and writing a 0 pulls the pin hard to ground. So "write 1" is the same as releasing an open-drain line, and the sensor can always pull SDA low against it. The firmware never actively drives a line high against the slave. The BMP280/BME280 never stretches the clock, so the master doesn't need to check SCL.

## Toolchain setup

Tested with SDCC 4.6.0 and avrdude 8.3 on Arch Linux.

```sh
sudo pacman -S sdcc avrdude make python
```

`packihx` comes with SDCC. Rebuilding the slides needs `chromium`.

To flash without `sudo`, add a udev rule for the USBasp. This one is already installed at `/etc/udev/rules.d/70-usbasp.rules` on the development machine:

```
SUBSYSTEM=="usb", ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="05dc", TAG+="uaccess"
```

Reload with `sudo udevadm control --reload && sudo udevadm trigger`, then replug the programmer.

Check that the programmer can see the chip:

```sh
avrdude -p 89S52 -c usbasp -v
```

It should report `Device signature = 1E 52 06 (AT89S52)`. The part name is `89S52`; `8052` is not valid. No custom avrdude config is needed.

## Building and flashing

| Command | What it does |
|---|---|
| `make` | Compiles `src/*.c` to `build/*.rel`, links `build/main.ihx`, and converts it to `build/main.hex` |
| `make flash` | Builds if needed, then erases and writes the chip with avrdude and verifies it |
| `make size` | Prints SDCC's internal RAM map, stack size and flash usage |
| `make clean` | Deletes `build/` |
| `make clean flash EXTRA=-DUART_DEBUG` | Debug build with serial prints (adds `uart.c`, about 400 bytes) |

Compiler flags: `-mmcs51 --model-small --code-size 8192 --iram-size 256 --std-sdcc11`. Anything in `EXTRA` is appended. When you change `EXTRA`, run `make clean` first, or the old object files are reused and the define has no effect.

The flash is nearly full. If you add anything, the linker fails with `Insufficient ROM/EPROM/FLASH memory`. A UART debug build is about 8.5 KB, so it only fits with something else removed; see [Memory budget](#memory-budget).

### Going back to the known-good build

```sh
git checkout demo-v1 && make clean flash    # stage 6 firmware, before the final-review features
git checkout main    && make clean flash    # back to the current firmware
```

`demo-v1` expects the old wiring (contrast PWM on P2.3, no external button), but it runs fine with the pot, and the external button is simply ignored.

### Flashing notes

- **Flashing is slow.** This programmer writes about 26 ms per byte. The current 8.1 KB image takes about 3.5 minutes plus verification. Don't wrap `make flash` in a short timeout, and don't touch the USB cable while it runs. A bumped cable ends with `Broken pipe` / `unable to write flash`.
- **`unable to write flash (rc = -1)` with no disconnect** happened once and went away on the next attempt. The chip is fine after any failed write; just flash again.
- **`Error: cannot set sck period; please check for usbasp firmware update`** is printed every time. It comes from the programmer's old firmware and is harmless. Don't pass `-B`, and don't try to update the programmer firmware.
- **Always let avrdude verify.** Chip erase on the 89S52 sometimes fails silently with USBasp clones. `make flash` verifies by default. Look for `NNNN bytes of flash verified` at the end.
- **If verify fails repeatedly**, fit the SCK jumper on the programmer (it slows down the ISP clock) and flash again.

## Using the altimeter

### Line 1

```
Alt:    16.5 m A
```

Altitude in metres with 0.1 m resolution, followed by the mode:

| Letter | Mode | Reference pressure |
|---|---|---|
| `R` | relative | the pressure when you last tapped the onboard button |
| `A` | absolute | QNH: 1013.25 hPa by default, or the value you entered |

Line 1 always uses the temperature-corrected formula. See [Temperature correction](#temperature-correction).

### Line 2 views

Tap the external button to cycle through them:

```
T 32.8C P1013.9      temperature (°C) and Kalman-filtered pressure (hPa)
ISA:     0.9 m       the same height as line 1, but with the 15 °C standard-atmosphere formula
V:     0.3 m/s       vertical speed from the Kalman filter, + = climbing
QNH 1013.2 hPa       the sea-level pressure absolute mode uses
```

The ISA view is there to show what the temperature correction does: at 32.8 °C, a 1 m lift reads about 1.0 m on line 1 and about 0.94 m in the ISA view. The QNH view shows the value truncated to 0.1 hPa, so the default 1013.25 shows as `1013.2`.

### Buttons

| Button | Tap (100 ms – 1 s) | Hold |
|---|---|---|
| Onboard, P3.3 | zero here: mode `R`, altitude 0.0 | 2 s: back to absolute mode `A` |
| External, P3.2 | next line-2 view | 1 s: open the QNH editor |

On the onboard button, a 2 s hold zeroes first (at 100 ms), then switches to absolute when you reach 2 s.

### Entering QNH

QNH is the current air pressure reduced to sea level. Every airport publishes it in its METAR weather report, updated every 30–60 minutes: in `... 31/24 Q1011 NOSIG`, `Q1011` means 1011 hPa. Use the nearest airport (aviationweather.gov or any METAR app), or a weather app's "sea-level pressure".

1. Hold the external button for 1 s. Line 1 shows `Set QNH (hPa)`, line 2 shows the current value, e.g. `1013.2`, with an underline cursor under the first digit.
2. Tap to increase the digit under the cursor (9 wraps to 0).
3. Hold 1 s to move to the next digit. The format is four digits, a decimal point, one digit: `0998.5` for 998.5 hPa.
4. On the last digit, hold 1 s to save. Absolute mode (`A`) is switched on with the new reference.

Values outside 870.0–1085.0 hPa (roughly the lowest and highest sea-level pressures ever recorded) show `Only 870-1085!` and put the cursor back on the first digit. There is no cancel: to keep the old value, re-enter it. The onboard button keeps working while the editor is open.

The QNH is held in RAM, so it resets to 1013.25 hPa on reset or power-off. The AT89S52 has no EEPROM.

### LED and startup

The display updates every 250 ms and the green LED toggles on each reading, so it blinks at 2 Hz while readings come in. If the LED stops, the firmware isn't getting readings.

At power-up, line 1 briefly shows `BMP280` or `BME280` (whichever chip was detected), then the readings replace it. If no sensor answers on 0x76 or 0x77, the LCD shows `No sensor` and the firmware stops. Fix the wiring and press reset.

### Demo

1. Put the board on a table and tap the onboard button. Line 1 shows `0.0 m R`.
2. Lift it straight up by about 1 m and hold it still for 2 s. It reads close to `1.0`.
3. Tap the external button to the ISA view: slightly lower, because it assumes 15 °C air.
4. Tap to the `V:` view and move the board up and down: positive going up, negative going down.
5. Hold the external button and enter today's QNH. Line 1 now shows the height above sea level.
6. Walk up a flight of stairs for a bigger change: one storey is about 3 m.

### UART output (debug build)

Built with `make clean flash EXTRA=-DUART_DEBUG` (which needs about 300 bytes freed elsewhere first), the firmware prints one line per reading at 9600 baud, 8N1:

```
T=3280 P=101390 Pk=101389
```

`T` is in 0.01 °C, `P` is the raw pressure in Pa, and `Pk` the Kalman-filtered pressure in Pa. Connect a USB-to-RS232 adapter to the board's DB9 and run `picocom -b 9600 /dev/ttyUSB0`. Not tested; no adapter was available.

## How it works

### Startup

`main()` in `src/main.c` runs these steps in order:

1. `uart_init()` (debug build only): Timer1 in mode 2 with TH1 = 0xFD and SMOD = 0, which gives 9600 baud from 11.0592 MHz.
2. `lcd_init()`: HD44780 4-bit initialisation.
3. `i2c_init()`: releases both lines and clocks out any sensor left mid-transfer by a reset.
4. `bme280_init()`: finds the sensor, resets it, reads the calibration and starts continuous measurement.
5. Waits 200 ms for the first sample, then starts the Timer2 tick with `tick_init()`.

### Main loop and timing

The loop runs on a fixed 50 ms tick from Timer2. Timer2 runs in 16-bit auto-reload mode with a reload value of 0x4C00 (46 080 machine cycles = 50.000 ms), and `tick_wait()` polls its overflow flag TF2. Because the timer reloads in hardware, the period stays exact no matter how long the work in each pass takes, as long as it's under 50 ms.

- **Every tick:** read both buttons, count how long each is held, and act on taps and holds.
- **Every fifth tick (exactly 250 ms):** read the sensor, run one Kalman filter step, then redraw the LCD.

The fixed period matters because the Kalman filter's motion model assumes a time step of exactly 0.25 s. For the same reason, a button press never triggers an extra sensor read: zeroing or changing the view only redraws the display from the current estimate.

### Sensor configuration

| Register | Value | Meaning |
|---|---|---|
| 0xE0 reset | 0xB6 | soft reset, then 5 ms wait |
| 0xF5 config | 0x2C | standby 62.5 ms, IIR filter coefficient 8, SPI 3-wire off |
| 0xF4 ctrl_meas | 0x57 | temperature oversampling x2, pressure oversampling x16, normal mode |

`config` is written before `ctrl_meas` because the datasheet says writes to `config` in normal mode may be ignored. In normal mode the sensor measures continuously by itself: about 43 ms per measurement plus 62.5 ms standby, so roughly 9.5 new samples per second. The firmware just reads the latest result.

The IIR filter was 16 until the Kalman filter went in. A heavy sensor-side filter delays every reading by a couple of seconds, and nothing after it can undo that. Coefficient 8 keeps some smoothing in the sensor and leaves the rest to the Kalman filter, which knows about motion.

The chip ID at 0xD0 decides the chip type: 0x60 is a BME280, 0x58 a BMP280. Both use identical temperature and pressure calibration and registers, so the same code handles both.

### Calibration and compensation

Each sensor is factory-trimmed with 12 calibration constants (`dig_T1..T3`, `dig_P1..P9`) stored little-endian in registers 0x88–0x9F. SDCC's 8051 port is also little-endian and doesn't pad structs, so `bme280_init()` reads all 24 bytes in one burst directly into a struct, with no byte shuffling.

The raw readings come from a 6-byte burst read at 0xF7–0xFC: 20-bit pressure, then 20-bit temperature. A raw value of 0x80000 means "no measurement yet", and that reading is skipped.

Temperature and pressure are converted with Bosch's 32-bit integer formulas from the datasheet (section 8.2, `bmp280_compensate_T_int32` and `bmp280_compensate_P_int32`). Temperature comes out in 0.01 °C, pressure in Pa with 1 Pa resolution. The 64-bit version would give finer resolution, but 64-bit arithmetic on an 8051 is too slow and too large.

This is the sensor's own temperature compensation: it corrects the pressure reading for the temperature of the chip. It's a different thing from the [temperature correction](#temperature-correction) of the altitude formula below.

Two SDCC-specific details matter here:

- `int` is 16 bits on SDCC, so every term in the formulas is cast to `int32_t` explicitly. Without the casts, intermediate results overflow silently.
- The two compensation functions are marked `__reentrant`. That moves their 32-bit local variables from the scarce direct RAM onto the stack. See [Memory budget](#memory-budget).

### Kalman filter

`src/kalman.c` replaces the plain running average used up to stage 6.

**Model.** The state is the pressure `p` (Pa) and its rate of change `v` (Pa/s), sampled every dt = 0.25 s. Between samples the board is assumed to keep moving at constant speed, disturbed by random acceleration (the "constant velocity, white-noise acceleration" model):

```
predict   p ← p + v·dt
          P ← F·P·Fᵀ + Q           F = [1 dt; 0 1]
update    S ← P00 + R
          K0 ← P00 / S,  K1 ← P01 / S
          y ← z − p                  z = measured pressure (innovation)
          p ← p + K0·y,  v ← v + K1·y
          P ← (I − K·H)·P            H = [1 0]
```

P is the 2x2 covariance (how uncertain the estimate is), Q the process noise (how hard the board might be moved), R the measurement noise (how noisy the sensor is). The gains K0 and K1 decide how much each new reading moves the estimate. The filter runs in full on the chip: P is updated every step, so at power-up, when P is large, the gains are large and the filter locks on quickly; as P shrinks, the gains settle to their steady state (K0 ≈ 0.37, K1 ≈ 0.33 /s).

**Why pressure and not altitude.** Altitude depends on the reference (the zero point or QNH), which jumps when you press a button. Filtering pressure means re-zeroing or entering a new QNH never disturbs the filter or produces a fake vertical-speed spike.

**Fixed point.** Everything is 32-bit integer:

| Quantity | Scaling |
|---|---|
| `kf_p`, `kf_v` (pressure, rate) | Q8: value × 256 |
| P00, P01, P11 | Q8 |
| K0, K1 | Q12: value × 4096 |
| dt = 1/4 s | right shifts by 2 |

The innovation is clamped to ±100 Pa so a glitch can't overflow the products. Signed right shifts rely on SDCC generating arithmetic shifts for signed `long`; this was checked in the generated assembly (`mov c,acc.7; rrc a`).

**Tuning.** From `tools/kalman_sim.py`:

| Parameter | Value |
|---|---|
| R (measurement noise) | σ = 0.71 Pa, so R = 0.504 Pa² (Q8: 129) |
| Random acceleration | 0.1 m/s², about 1.13 Pa/s² |
| Q00, Q01, Q11 | 0.0013, 0.0100, 0.0803 (Q8: 0, 3, 21) |
| Initial P00, P11 | 100 Pa², 100 (Pa/s)² |

Simulated results for a 1 m lift (sensor modelled with 2.5 Pa raw noise, its IIR filter and 1 Pa rounding):

| | Old: sensor IIR ×16 + running average | New: sensor IIR ×8 + Kalman |
|---|---|---|
| Jitter at rest | 3.8 cm | 5.9 cm |
| Lag behind a 1 m lift (50% crossing) | 3.25 s | 1.0 s |
| Vertical speed | none | ±2.1 cm/s noise at rest |

Both jitter figures are below the 0.1 m display step. The simulator's integer copy matches the float filter within 0.32 cm and 0.31 cm/s. To retune, edit `SIGMA_Z` and `A_MPS2` in the simulator, run it, and paste the output of `--consts` into `src/kalman.c`.

The vertical speed is smoothed too: a brisk 1 s lift peaking at 1.57 m/s shows only about 0.4 m/s. It's most useful for steady climbs like stairs.

### Temperature correction

The standard-atmosphere formula `h = 44330 × (1 − (P/101325)^0.1903)` assumes air at 15 °C at sea level, cooling with height. Warmer air is less dense, so the same pressure drop corresponds to more height. Line 1 uses the hypsometric equation with the measured temperature instead:

```
h = (R / (M·g)) · T · ln(p0 / p)        R / (M·g) = 29.27 m/K, T in kelvin
```

This assumes one temperature for the whole layer of air between the two levels, which is fine for anything from a table top to a tall building.

The 8051 can't afford `ln`, but `ln(p0/p) = 2·artanh(x)` with `x = (p0 − p)/(p0 + p)`, and `artanh(x) ≈ x` for small x. So:

```
h ≈ 2 · 29.27 · T · (p0 − p) / (p0 + p)
```

In integers, with `t` in 0.01 °C and the result in 0.1 m, `2 · 29.27 · 10 · T = 5.854 · (t + 27315)`. `altitude_hyps_dm()` in `src/altitude.c` computes:

```c
k  = 5854L * (t + 27315L) / 8000;          /* = H / 8, about 22 300 at 30 °C */
dp = p0 - p;                               /* clamped to ±80 000 */
h  = (k * dp ± d/2) / d,  d = (p0 + p) / 8 /* rounded to nearest */
```

Dividing both `k` and `p0 + p` by 8 keeps `k · dp` inside 32 bits. `main.c` passes pressures in units of ¼ Pa (Q2 from the Kalman filter's Q8), so the clamp corresponds to ±20 000 Pa, about ±1.7 km between the reading and the reference.

From `tools/check_hyps.py`: the integer version matches the exact formula within 0.09 m up to ±300 m, and for the same pressure drop:

| Air temperature | Standard formula | Corrected |
|---|---|---|
| 0 °C | 99.5 m | 94.4 m |
| 15 °C | 99.5 m | 99.6 m |
| 25 °C | 99.5 m | 103.0 m |
| 32.8 °C | 99.5 m | 105.7 m |
| 40 °C | 99.5 m | 108.2 m |

At the 32.8 °C measured in the lab, the standard formula reads 5.9% low.

Vertical speed uses the same function: `altitude_hyps_dm(p + v, p, t)` is the height gained in one second at the current pressure rate, so it comes out in 0.1 m/s.

### Standard-atmosphere table (ISA view)

Computing `(P/P0)^0.1903` with SDCC's float `powf` made the firmware about 7.3 KB and overflowed direct RAM, so it's precomputed.

`tools/gen_alt_table.py` evaluates the formula every 1024 Pa from 86 016 Pa (about +1.3 km) to 106 496 Pa (about −430 m): 21 points. They're stored in `src/altitude.c` as `const __code uint16_t` values in 0.1 m with an offset of 4500 (450.0 m) added, so they fit in 16 bits. `const __code` keeps them in flash; without it SDCC would copy the table into RAM at startup.

`altitude_dm()` finds the two neighbouring points and interpolates linearly. Because the step is 1024 = 2^10, the index is `(p − PMIN) >> 10` and the fraction `(p − PMIN) & 1023`, so there's no 32-bit division. Pressures outside the table clamp to its ends.

The ISA view shows `altitude_dm(p) − altitude_dm(p0)`. Interpolation error from `python3 tools/gen_alt_table.py --check`:

| Pressure | Exact | Table |
|---|---|---|
| 101325 Pa | 0.00 m | 0.1 m |
| 100000 Pa | 110.9 m | 111.0 m |
| 90000 Pa | 988.67 m | 988.7 m |
| 87000 Pa | 1267.39 m | 1267.4 m |

The worst error over the range is 0.17 m. In the ISA view both terms carry nearly the same error, so their difference is better than that.

Until the final review the table covered 50–110 kPa in 1000 Pa steps (61 `int32_t` entries). It was cut to the current form to fit the new features into 8 KB.

To change it, edit `SHIFT`, `PMIN`, `PMAX` and `OFFSET` in both `tools/gen_alt_table.py` and `src/altitude.c`, then:

```sh
python3 tools/gen_alt_table.py --check    # accuracy report
python3 tools/gen_alt_table.py            # C initializer to paste into src/altitude.c
```

### Absolute and relative modes

`main.c` keeps a reference pressure `p0_q2` (¼ Pa units):

- **Onboard tap:** `p0_q2` = the current Kalman pressure, mode `R`.
- **Onboard 2 s hold:** `p0_q2` = QNH × 4, mode `A`.
- **QNH saved:** `qnh` = entered value × 10 Pa, `p0_q2` = QNH × 4, mode `A`.

Line 1 is `altitude_hyps_dm(p, p0_q2, t)`. In absolute mode this treats the air between sea level and you as one temperature; near sea level the difference from a full lapse-rate model is negligible.

### Number formatting

There's no `printf`; SDCC's version is several KB. `lcd_tenths()` in `main.c` prints a fixed-point value in tenths, right-aligned to a given width, with a minus sign when negative. `uart_put_i32()` and `uart_put_hex8()` in `uart.c` do the same job for the serial port.

### I2C

`src/i2c.c` is a single-master bit-bang driver. Each half clock period is a `delay_us(1)` call, which with call overhead comes to several microseconds, so the bus runs at a few tens of kHz (not measured), well inside the sensor's 100/400 kHz limits.

- `i2c_init()` checks whether SDA is stuck low (a sensor left mid-byte by an 8051 reset) and clocks SCL up to 9 times to free it, then sends a START and STOP.
- `i2c_write()` returns 1 if the slave acknowledged.
- `i2c_read(ack)` sends ACK for every byte except the last, which gets NACK.
- `i2c_read_regs()` writes the register address, sends a repeated START, then reads `n` bytes.
- `i2c_probe()` sends just the address byte and reports whether anything acknowledged.

### LCD driver

`src/lcd.c` drives the HD44780 in 4-bit mode. The pins are written one bit at a time (`P2_4 = ...`) instead of writing all of Port 2, so the LED on P2.0 and the free P2.3 are never disturbed.

Initialisation follows the datasheet's "initialisation by instruction" sequence: wait 50 ms, send nibble 0x3 three times (5 ms, 1 ms, 1 ms apart), then 0x2 to switch to 4-bit mode, then 0x28 (4-bit, 2 lines, 5x8 font), 0x0C (display on, cursor off), 0x06 (increment) and 0x01 (clear). Ordinary commands and characters get a 50 µs delay; clear and home get 2 ms. The QNH editor turns the underline cursor on with 0x0E and off again with 0x0C.

### Buttons

Both buttons are polled every 50 ms tick with their own hold counters.

- **Onboard (P3.3):** acts while held. At 2 ticks (100 ms) it zeroes; at 40 ticks (2 s) it switches to absolute mode.
- **External (P3.2):** `btn2_event()` reports a tap on release if the button was held for 2–19 ticks (100 ms to under 1 s), or a hold once when the count reaches 20 ticks (1 s). A release after a hold is ignored, so one long press never also counts as a tap.

The 2-tick minimum is the debounce.

### Delays

`delay_ms()` in `src/delay.c` uses Timer0 in 16-bit mode, reloaded with 0xFC66 (922 machine cycles, about 1.000 ms) for each millisecond. It changes only the Timer0 half of TMOD, so Timer1's UART setting is left alone. It's used only during startup.

`delay_us()` is a plain busy loop that always runs longer than requested. It's used where a minimum delay is needed (LCD writes, I2C).

`tick_init()` / `tick_wait()` provide the 50 ms main-loop tick on Timer2, described under [Main loop and timing](#main-loop-and-timing).

### UART

`src/uart.c` sets Timer1 to 8-bit auto-reload with TH1 = 0xFD and SMOD = 0, which gives 9600 baud with an 11.0592 MHz crystal (that's why the board uses that odd frequency). Transmission is polled: `uart_putc()` waits for TI before writing SBUF. `TI` is set to 1 at init so the first character doesn't wait forever. It's linked in only when `EXTRA` contains `UART_DEBUG`.

## Memory budget

The AT89S52 has 8 KB of flash and 256 bytes of internal RAM. There's no external RAM on this board, so `__xdata`, `__pdata`, `--model-medium` and `--model-large` must not be used. They link fine and then fail at runtime.

Both are now nearly full: 8118 of 8192 bytes of flash, and 63 bytes of stack.

### Flash

| Part | Bytes |
|---|---|
| `bme280.c` | 2549 |
| `main.c` | 2093 |
| `altitude.c` (both formulas + 42-byte table) | 897 |
| runtime library (32-bit multiply/divide) and startup code | 878 |
| `kalman.c` | 874 |
| `i2c.c` | 500 |
| `lcd.c` | 249 |
| `delay.c` | 78 |

With all the final-review features in, the first build was 8415 bytes, 223 over. What was tried:

| Change | Effect |
|---|---|
| `--opt-code-size` | 0 bytes saved |
| UART prints moved to a debug build | −386 bytes (done before the Kalman filter) |
| Kalman: one shared multiply-and-shift helper instead of five inlined copies | about −110 bytes |
| Table as `uint16_t` with an offset instead of `int32_t` | about −80 bytes |
| Table step 1024 Pa (shifts instead of division) and range cut to −430 m…+1.3 km | about −110 bytes |
| Compensation functions not `__reentrant` | −164 bytes, but needs 44 more bytes of direct RAM that don't exist; reverted |
| Hypsometric function not `__reentrant` | −92 bytes, costs 20 bytes of direct RAM; reverted |

If more space is needed, the cheapest cut is the QNH view on line 2 (about 60 bytes; the editor shows the value anyway). The next is a steady-state Kalman filter with fixed gains from the simulator (about 650 bytes), at the cost of not updating the covariance on the chip.

### RAM

| Region | Addresses | Addressing | Used for |
|---|---|---|---|
| Register bank 0 | 0x00–0x07 | direct | R0–R7 |
| Bit-addressable | 0x20–0x2F | direct / bit | `__bit` variables |
| Direct RAM | 0x08–0x7F | direct | globals and the static local variables of every non-reentrant function (`DSEG`) |
| Indirect RAM | 0x80–0xFF (and leftovers below) | `@R0`/`@R1` only | `__idata` variables (`ISEG`) and the stack |

In SDCC's small model, every local variable of an ordinary function gets a fixed static slot in direct RAM, and the whole `DSEG` must fit in one contiguous block below 0x80. The first build with float and the 32-bit formulas needed 125 bytes there and failed to link with `Could not get NN consecutive bytes in internal RAM for area DSEG`. These keep it in budget:

| Change | Where |
|---|---|
| Calibration constants in an `__idata` struct | `bme280.c` |
| `t_fine` and the 6-byte raw-data buffer in `__idata` | `bme280.c` |
| `compensate_T` / `compensate_P` and `altitude_hyps_dm` marked `__reentrant`, so their locals live on the stack | `bme280.c`, `altitude.c` |
| Kalman state and covariance in `__idata` | `kalman.c` |
| QNH digits and formatting buffers in `__idata` | `main.c` |
| Lookup table instead of `powf` | `altitude.c` |

`make size` prints the RAM map. The legend: `a`–`z` are each module's direct data, `B` bits, `Q` overlay, `I` `__idata`, `S` stack. The deepest call chain (main → sensor read → reentrant compensation → 32-bit library) needs roughly 40 bytes of stack, so 63 bytes is enough but leaves little margin. Put new buffers in `__idata` and check `make size` after every change.

## Source tour

```
.
├── Makefile                  build, flash, size, clean
├── CLAUDE.md                 project notes and hardware rules
├── README.md
├── src/
│   ├── main.c                startup, main loop, buttons, views, QNH editor
│   ├── delay.c/.h            delay_ms (Timer0), delay_us, 50 ms tick (Timer2)
│   ├── uart.c/.h             9600 8N1 polled output (debug build only)
│   ├── lcd.c/.h              HD44780 4-bit driver
│   ├── i2c.c/.h              bit-banged I2C master, register read/write helpers
│   ├── bme280.c/.h           sensor detection, configuration, calibration, Bosch compensation
│   ├── kalman.c/.h           2-state Kalman filter on pressure, fixed point
│   └── altitude.c/.h         hypsometric formula, standard-atmosphere table
├── tools/
│   ├── gen_alt_table.py      generates and checks the standard-atmosphere table
│   ├── check_hyps.py         checks the integer hypsometric formula
│   └── kalman_sim.py         Kalman simulator: tuning, float reference, bit-exact C copy
└── deck/
    ├── slides.html           final-review slides (source)
    ├── fonts/                IBM Plex Sans / Mono (SIL Open Font License)
    └── altimeter-final-review.pdf
```

### Public functions

| Module | Function | Purpose |
|---|---|---|
| delay | `delay_ms(ms)` | accurate millisecond delay on Timer0 |
| delay | `delay_us(us)` | at least `us` microseconds, rough |
| delay | `tick_init()`, `tick_wait()` | 50 ms loop tick on Timer2 |
| uart | `uart_init()`, `uart_putc()`, `uart_puts()` | setup and output |
| uart | `uart_put_i32()`, `uart_put_hex8()`, `uart_crlf()` | number formatting |
| lcd | `lcd_init()`, `lcd_cmd()`, `lcd_putc()`, `lcd_puts()` | setup and output |
| lcd | `lcd_goto(col, row)`, `lcd_clear()` | cursor position, clear screen |
| i2c | `i2c_init()`, `i2c_start()`, `i2c_stop()`, `i2c_write()`, `i2c_read()` | bus primitives |
| i2c | `i2c_probe(addr)`, `i2c_write_reg()`, `i2c_read_regs()` | 7-bit address helpers, return 1 on success |
| bme280 | `bme280_init()` | returns chip ID (0x60 / 0x58) or 0 if not found |
| bme280 | `bme280_read(&t, &p)` | temperature in 0.01 °C, pressure in Pa, returns 1 on success |
| kalman | `kalman_init(z)`, `kalman_step(z)` | start / advance the filter with a pressure in Pa |
| kalman | `kf_p`, `kf_v` | filtered pressure and rate, ×256 |
| altitude | `altitude_hyps_dm(p, p0, t)` | temperature-corrected height of p above p0, in 0.1 m |
| altitude | `altitude_dm(p)` | standard-atmosphere altitude in 0.1 m (table) |

### Tunable constants

| Constant | File | Default | Effect |
|---|---|---|---|
| `SAMPLE_TICKS` | main.c | 5 | ticks per sensor reading (5 × 50 ms = 250 ms); the Kalman constants assume 0.25 s |
| `TAP_TICKS` | main.c | 2 | minimum press, 100 ms (debounce) |
| `HOLD_ABS_TICKS` | main.c | 40 | onboard hold for absolute mode, 2 s |
| `HOLD2_TICKS` | main.c | 20 | external hold for the editor / next digit, 1 s |
| `QNH_MIN`, `QNH_MAX` | main.c | 8700, 10850 | accepted QNH range in 0.1 hPa |
| `P0_STD` | main.c | 101325 | default QNH in Pa |
| `KF_R`, `KF_Q00`… | kalman.c | from the simulator | filter tuning (Q8) |
| `CONFIG_VAL` | bme280.c | 0x2C | sensor standby and IIR filter |
| `CTRL_MEAS_VAL` | bme280.c | 0x57 | sensor oversampling and mode |
| `T2_TICK_H/L` | delay.c | 0x4C00 | Timer2 reload for 50 ms |

## Host-side tools

All three run with plain Python 3, no packages.

| Command | What it shows |
|---|---|
| `python3 tools/check_hyps.py` | integer hypsometric formula vs exact; ISA vs corrected at several temperatures |
| `python3 tools/gen_alt_table.py --check` | table interpolation vs the exact standard-atmosphere formula |
| `python3 tools/gen_alt_table.py` | the C initializer for the table |
| `python3 tools/kalman_sim.py` | Kalman tuning report: gains, jitter, lag, vertical-speed noise, fixed vs float |
| `python3 tools/kalman_sim.py A_MPS2=0.2 SIGMA_Z=0.9` | the same with other tuning (any module constant can be overridden) |
| `python3 tools/kalman_sim.py --consts` | the Q8 constants to paste into `src/kalman.c` |

The integer code in `check_hyps.py` and `kalman_sim.py` mirrors the C line by line, including C's truncating division and SDCC's arithmetic right shift, so a passing check means the 8051 computes the same numbers.

## Accuracy and limitations

- **Absolute accuracy is about ±10 m.** The BMP280's absolute pressure accuracy is ±1 hPa (about ±8 m), and METAR QNH is rounded to 1 hPa. Relative accuracy is much better: the sensor is ±0.12 hPa relative, and the display steps are 0.1 m.
- **Weather drift.** Atmospheric pressure changes by up to a few hPa over hours, and every 0.1 hPa looks like about 0.8 m. A zero set in the morning can drift by several metres by evening. Tap again to re-zero, or update the QNH.
- **One temperature for the whole air column.** The correction uses the current sensor temperature as the mean temperature between the two levels. Fine for building heights; for mountains, a lapse-rate model would be better.
- **Sensor temperature reads high.** The sensor sits on a breakout near the board and warms slightly. Each 1 K of error is about 0.3% of the height.
- **Kalman tuning comes from simulation**, not from logged sensor data (no serial adapter). The real sensor noise, including air currents from fans, may differ.
- **Vertical speed is smoothed**: short, fast moves show a lower peak than the real speed.
- **ISA view range**: the table covers about −430 m to +1.3 km and clamps outside it. Line 1 is unaffected (its limit is ±1.7 km between the reading and the reference).
- **No persistence.** The zero reference and QNH reset on power-off; the AT89S52 has no EEPROM.
- **No sensor recovery.** If the sensor isn't found at startup the firmware stops at `No sensor`. If it disconnects later, the display freezes on the last reading (the LED stops blinking) until reset.
- **Flash is full** (74 bytes left).
- **Humidity is not used**, even on a real BME280.

## Barometric vs GPS altitude

A phone GPS app once showed −59.7 m "MSL" while the altimeter read about 16.5 m in absolute mode with the default 1013.25 hPa. They measure different things:

| Height | Measured from | Typical error |
|---|---|---|
| GPS, raw | the WGS84 ellipsoid, a smooth mathematical model of the Earth | ±10–30 m vertically, worse indoors |
| Mean sea level | the geoid; around South India it lies roughly 60–100 m below the ellipsoid | depends on the app applying a geoid model |
| Pressure altitude (`A`, 1013.25 hPa) | the level where pressure would be 1013.25 hPa | about 8 m per hPa of weather |
| QNH altitude (`A`, QNH entered) | sea level, via today's QNH | about ±10 m |

The phone's −59.7 m is consistent with a raw ellipsoid height that hasn't been corrected for the geoid, plus normal GPS error. For a reliable reference elevation, use a survey benchmark or a topographic map, not phone GPS.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| avrdude: `cannot set sck period` | Old programmer firmware | Ignore it |
| avrdude can't find the programmer | Not plugged in, or no udev rule | Check `lsusb` for `16c0:05dc`, install the udev rule |
| `Broken pipe` / `unable to write flash` | USB cable moved during the write, or a one-off glitch | Flash again, don't touch the cable |
| Flash "hangs" | It's just slow: about 3.5 min for 8 KB | Wait; don't use a short timeout |
| Verify mismatch | Erase failed silently, or ISP clock too fast | Flash again; if it persists, fit the programmer's SCK jumper |
| Linker: `Insufficient ROM/EPROM/FLASH memory` | Over 8 KB | See [Memory budget](#memory-budget) |
| Linker: `Could not get NN consecutive bytes in internal RAM for area DSEG` | Too many globals or locals in direct RAM | Move buffers to `__idata`, mark big 32-bit functions `__reentrant` |
| `make EXTRA=...` has no effect | Old objects reused | `make clean` first |
| LCD completely blank | Contrast pot turned too far, LCD unpowered, or RW floating | Turn the pot; check pins 1, 2 and 5 |
| LCD grey all over | V0 too close to GND | Turn the pot the other way |
| Top row solid blocks, no text | LCD powered but not initialised: RS/E/D4–D7 wiring | Check the LCD table |
| Garbage characters | Data lines swapped, or a loose D4–D7 wire | Check the LCD table |
| `No sensor` | Sensor not on 0x76/0x77, or SDA/SCL swapped or loose | Check the sensor wiring, then reset |
| Temperature like −140 °C or hundreds of degrees | Calibration read wrong | Shouldn't happen with this code; check the I2C wiring |
| LED not blinking | Firmware stuck or no sensor readings | Reset; check the sensor wiring |
| Onboard button does nothing | JP2 jumper missing | Fit JP2 |
| External button does nothing | Not between P3.2 and GND, or a loose wire | Check the wiring |
| Tap on the external button acts as a hold | Held for 1 s or more | Release sooner; taps are 100 ms to 1 s |
| `Only 870-1085!` | QNH out of range, e.g. leading digit not 0 or 1 | Re-enter, e.g. `1011.0` |
| Absolute altitude tens of metres off | QNH not set, or old | Enter the current QNH |
| Altitude drifts slowly while standing still | Weather | Re-zero |
| LED never lights | JP3 jumper missing | Fit JP3 |

## Development history

The firmware was brought up one stage at a time, each flashed and checked on the board before moving on:

| Commit | Stage | Check on hardware |
|---|---|---|
| `08a1f2e` | Makefile, Timer0 delay, LED blink | LED blinks at about 1 Hz |
| `5268ac4` | UART "hello" | Not checked: no serial adapter |
| `83aae19` | HD44780 4-bit LCD driver | Text and a counter on the LCD (RW and V0 had to be tied to GND first) |
| `e95aa60` | Bit-banged I2C, bus scan, chip ID | `I2C: 76`, `ID 58 BMP280` |
| `644b2b1` | Compensation and table-based altitude | 32.8 °C, 1013.9 hPa, −5.5 m |
| `67869c7` | Zero button, running average, absolute/relative modes | Zero, 1 m lift and 2 s hold all work |
| `7f41e3a` | LCD contrast PWM on P2.3 | Readable straight on at 40%, no flicker |
| `4c611c1` | README (tagged `demo-v1`) | |
| `a85b940` | PWM removed (10k pot fitted), Timer2 as the 50 ms loop tick | Display fine, updates at 4 Hz |
| `90335b6` | Temperature-corrected altitude, UART made optional | 1 m lift reads 1.0–1.1 m |
| `ca5da7c` | Kalman filter, external button, views, QNH editor | All views, vertical speed and the editor work |
| `c3deef7` | Final-review slide deck | |

### Problems along the way

- **The LCD showed nothing.** V0 (contrast) and RW were both unconnected. Tying both to GND made text appear, but only at a steep angle.
- **No pot or suitable resistor for contrast.** The 10k and 220k resistors on hand lifted V0 too far (the LCD's internal ladder is about 10k) and blanked the screen. For a while V0 was driven from P2.3 by software PWM: a naked assembly Timer2 interrupt switched the pin at 3.6 kHz, released 40% of the time, tuned with a sweep build. It used about 10% of the CPU. When a 10k pot turned up, the PWM was removed and Timer2 became the loop tick.
- **An SDCC interrupt gotcha** found along the way: SDCC builds the interrupt vector table only in the file containing `main()`, and only for handlers whose prototypes it can see there. Without the prototype, the build succeeds but the vector is missing, and the first interrupt jumps into unrelated code.
- **Float `powf` didn't fit.** It pushed flash to about 7.3 KB and overflowed direct RAM, so it was replaced by a lookup table.
- **The linker ran out of direct RAM** even after removing float, because SDCC gives each local variable a static slot. Fixed with `__idata` and `__reentrant`.
- **Flashing seemed to hang.** It was just slow: about 26 ms per byte. Short command timeouts killed it partway through.
- **The sensor is a BMP280**, not the BME280 it was sold as. The code handles both.
- **The final-review features didn't fit in 8 KB** at first (8415 bytes). Each saving was measured before choosing; see [Memory budget](#memory-budget).
- **The Kalman filter couldn't be faster than the sensor.** The first simulation showed the sensor's own IIR ×16 filter delayed a step by about 4 s on its own, so it was lowered to ×8 and the Kalman filter does the rest.
