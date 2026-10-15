# 8051 barometric altimeter

An AT89S52 reads a BMP280 pressure sensor over bit-banged I2C, converts the pressure to altitude, and shows altitude, temperature and pressure on a 16x2 character LCD. A push button zeroes the altitude at the current height, so you can lift the board 1 m and watch it read +1.0 m.

This is an MPMC (Microprocessors and Microcontrollers) course project. Everything is written in C for SDCC on Arch Linux and flashed with a USBasp-compatible programmer through avrdude. No Keil, no Windows.

```
Alt:     1.0 m R     <- altitude in metres, R = relative (zeroed), A = absolute
T 32.8C P1013.9      <- temperature in °C, pressure in hPa
```

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
- [Accuracy and limitations](#accuracy-and-limitations)
- [Troubleshooting](#troubleshooting)
- [Development history](#development-history)

## Status

Tested on the real board:

| Feature | Result |
|---|---|
| LED heartbeat on P2.0 | works |
| LCD in 4-bit mode | works, readable straight on with PWM contrast |
| I2C scan | sensor found at 0x76 |
| Chip ID | 0x58, a BMP280 (sold as a BME280, which is common) |
| Compensated temperature and pressure | 32.8 °C, 1013.9 hPa in a warm room, consistent with the −5.5 m absolute reading |
| Zero button | tap zeroes, hold 2 s returns to absolute mode |
| Lift test | about +1 m when raised 1 m after zeroing, back to about 0 when lowered |

Not tested:

| Feature | Why |
|---|---|
| UART debug output | No USB-to-RS232 adapter is available. The "2-in-1" programmer is only a USBasp (USB ID 16c0:05dc) and has no serial port. The code is in place and prints every reading. |
| Humidity | The fitted sensor is a BMP280, which has no humidity sensor. The firmware does not read humidity on a real BME280 either. |

Current build: 6402 of 8192 bytes of flash, stack starts at 0x98 with 104 bytes free.

## Hardware

| Part | Details |
|---|---|
| Board | MyTechnoCare "My 8051" development board, MY010V2.1 (borrowed, so nothing in this project risks damaging it) |
| MCU | Atmel AT89S52: 8 KB flash, 256 bytes internal RAM, three timers, no EEPROM, no external RAM fitted |
| Clock | 11.0592 MHz crystal, 12 clocks per machine cycle, so one machine cycle is 1.085 µs |
| Sensor | 5 V BMP280/BME280 breakout with an onboard 3.3 V regulator and I2C level shifter |
| Display | 16x2 HD44780-compatible character LCD with backlight |
| Programmer | MyTechnoCare 2-in-1 USB ISP programmer, a USBasp clone with old firmware |
| Onboard extras used | Green LED on P2.0 (jumper JP3), push button on P3.3 / INT1 (jumper JP2), MAX232 on P3.0/P3.1 |

## Wiring

### Pin map

| Signal | 8051 pin | Notes |
|---|---|---|
| I2C SCL | P1.0 | bit-banged |
| I2C SDA | P1.1 | bit-banged |
| LCD RS | P2.1 | |
| LCD E | P2.2 | |
| LCD V0 (contrast) | P2.3 | software PWM, see [LCD contrast](#lcd-contrast-without-a-pot) |
| LCD D4..D7 | P2.4..P2.7 | 4-bit mode |
| Status LED | P2.0 | onboard, toggles on every sensor reading |
| Zero button | P3.3 | onboard, active low |
| UART TX / RX | P3.1 / P3.0 | onboard MAX232 to the DB9 connector, 9600 8N1 |

Port 0 is not used at all. On this board it's open drain with no pull-up resistors fitted, so it can't drive anything high.

### LCD (16-pin header)

| LCD pin | Name | Connect to |
|---|---|---|
| 1 | VSS | GND |
| 2 | VDD | 5 V |
| 3 | V0 | P2.3 |
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

### Why bit-banged I2C is safe on Port 1

Port 1 is quasi-bidirectional: writing a 1 turns on only a weak internal pull-up, and writing a 0 pulls the pin hard to ground. So "write 1" is the same as releasing an open-drain line, and the sensor can always pull SDA low against it. The firmware never actively drives a line high against the slave. The BMP280/BME280 never stretches the clock, so the master doesn't need to check SCL.

## Toolchain setup

Tested with SDCC 4.6.0 and avrdude 8.3 on Arch Linux.

```sh
sudo pacman -S sdcc avrdude make python
```

`packihx` comes with SDCC.

To flash without `sudo`, add a udev rule for the USBasp. This one is already installed at `/etc/udev/rules.d/70-usbasp.rules` on the development machine:

```
SUBSYSTEM=="usb", ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="05dc", TAG+="uaccess"
```

Reload with `sudo udevadm control --reload && sudo udevadm trigger`, then replug the programmer.

Check that the programmer can see the chip:

```sh
avrdude -p 89S52 -c usbasp
```

It should report `Device signature = 1E 52 06 (AT89S52)`. The part name is `89S52`; `8052` is not valid. No custom avrdude config is needed.

## Building and flashing

| Command | What it does |
|---|---|
| `make` | Compiles every file in `src/` to `build/*.rel`, links `build/main.ihx`, and converts it to `build/main.hex` |
| `make flash` | Builds if needed, then erases and writes the chip with avrdude and verifies it |
| `make size` | Prints SDCC's internal RAM map, stack size and flash usage |
| `make clean` | Deletes `build/` |
| `make clean flash EXTRA=-DCONTRAST_SWEEP` | Flashes the contrast tuning build (see [LCD contrast](#lcd-contrast-without-a-pot)) |

Compiler flags: `-mmcs51 --model-small --code-size 8192 --iram-size 256 --std-sdcc11`. Anything in `EXTRA` is appended. When you change `EXTRA`, run `make clean` first. Otherwise the old object files are reused and the define has no effect.

### Flashing notes

- **Flashing is slow.** This programmer writes about 26 ms per byte. The current 6.4 KB image takes roughly 3 minutes including verification. Don't wrap `make flash` in a short timeout, and don't touch the USB cable while it runs. A bumped cable during a write ends with `Broken pipe` / `unable to write flash`. The chip is fine afterwards; just flash again.
- **`Error: cannot set sck period; please check for usbasp firmware update`** is printed every time. It comes from the programmer's old firmware and is harmless. Don't pass `-B`, and don't try to update the programmer firmware.
- **Always let avrdude verify.** Chip erase on the 89S52 sometimes fails silently with USBasp clones. `make flash` verifies by default. Look for `NNNN bytes of flash verified` at the end.
- **If verify fails**, fit the SCK jumper on the programmer (it slows down the ISP clock) and flash again.

## Using the altimeter

### Display

```
Alt:    -5.5 m A
T 32.8C P1013.9
```

| Field | Meaning |
|---|---|
| `Alt:` value | Altitude in metres, 0.1 m resolution, right-aligned in 8 characters |
| `A` | Absolute mode: altitude above the standard sea-level pressure of 1013.25 hPa |
| `R` | Relative mode: altitude above the point where you last pressed the button |
| `T` | Sensor temperature in °C, 0.1 °C resolution |
| `P` | Smoothed pressure in hPa, 0.1 hPa resolution |

The display updates every 250 ms and the green LED toggles on each update, so it blinks at 2 Hz while readings are coming in. If the LED stops, the firmware isn't getting readings.

At power-up, line 1 briefly shows `BMP280` or `BME280` (whichever chip was detected), then the readings replace it. If no sensor answers on 0x76 or 0x77, the LCD shows `No sensor` and the firmware stops. Fix the wiring and press reset.

### Button

| Action | Result |
|---|---|
| Tap (held at least ~100 ms) | Zero: the current smoothed pressure becomes the reference, mode switches to `R`, altitude reads 0.0 m |
| Hold 2 s | Back to absolute mode (`A`), reference returns to 1013.25 hPa |

A 2 s hold zeroes first, then switches to absolute when you reach 2 s. The zero reference is lost on reset or power-off, because the AT89S52 has no EEPROM.

### Demo

1. Put the board on a table and tap the button. Line 1 shows `0.0 m R`.
2. Lift it straight up by about 1 m and hold it still for 3 s. It should read close to `1.0`.
3. Put it back down. It returns to about `0.0`.
4. Walk up a flight of stairs for a bigger change: one storey is about 3 m.

### UART output

At 9600 baud, 8N1, the firmware prints one line per reading:

```
T=3280 P=101390 alt=-55A
```

`T` is in 0.01 °C, `P` is the raw (unsmoothed) pressure in Pa, and `alt` is in 0.1 m followed by the mode letter. To see it, connect a USB-to-RS232 adapter to the board's DB9 connector and run `picocom -b 9600 /dev/ttyUSB0`. This has not been tested because no adapter was available.

## How it works

### Startup

`main()` in `src/main.c` runs these steps in order:

1. `contrast_init(40)`: starts the Timer2 PWM on P2.3 so the LCD is readable from the first frame.
2. `uart_init()`: Timer1 in mode 2 with TH1 = 0xFD and SMOD = 0, which gives 9600 baud from 11.0592 MHz.
3. `lcd_init()`: HD44780 4-bit initialisation.
4. `i2c_init()`: releases both lines and clocks out any sensor left mid-transfer by a reset.
5. `bme280_init()`: finds the sensor, resets it, reads the calibration and starts continuous measurement.
6. Waits 200 ms for the first filtered sample, then enters the main loop.

### Main loop

The loop runs every 50 ms (Timer0-based `delay_ms(50)`):

- **Every pass:** checks the button and counts how long it has been held.
- **Every fifth pass (250 ms):** reads the sensor, updates the running average, computes altitude, and redraws both LCD lines and the UART line.

Polling at 50 ms keeps the button responsive while the slower sensor/LCD work happens at 4 Hz.

### Sensor configuration

| Register | Value | Meaning |
|---|---|---|
| 0xE0 reset | 0xB6 | soft reset, then 5 ms wait |
| 0xF5 config | 0x30 | standby 62.5 ms, IIR filter coefficient 16, SPI 3-wire off |
| 0xF4 ctrl_meas | 0x57 | temperature oversampling x2, pressure oversampling x16, normal mode |

`config` is written before `ctrl_meas` because the datasheet says writes to `config` in normal mode may be ignored. In normal mode the sensor measures continuously by itself: about 43 ms per measurement plus 62.5 ms standby, so roughly 9.5 new samples per second. The firmware just reads the latest result.

The chip ID at 0xD0 decides the chip type: 0x60 is a BME280, 0x58 a BMP280. Both use identical temperature and pressure calibration and registers, so the same code handles both.

### Calibration and compensation

Each sensor is factory-trimmed with 12 calibration constants (`dig_T1..T3`, `dig_P1..P9`) stored little-endian in registers 0x88–0x9F. SDCC's 8051 port is also little-endian and doesn't pad structs, so `bme280_init()` reads all 24 bytes in one burst directly into a struct, with no byte shuffling.

The raw readings come from a 6-byte burst read at 0xF7–0xFC: 20-bit pressure, then 20-bit temperature. A raw value of 0x80000 means "no measurement yet", and that reading is skipped.

Temperature and pressure are converted with Bosch's 32-bit integer formulas from the datasheet (section 8.2, `bmp280_compensate_T_int32` and `bmp280_compensate_P_int32`). Temperature comes out in 0.01 °C. Pressure comes out in Pa with 1 Pa resolution, which is about 8 cm of altitude. The 64-bit version would give finer resolution, but 64-bit arithmetic on an 8051 is too slow and too large.

Two SDCC-specific details matter here:

- `int` is 16 bits on SDCC, so every term in the formulas is cast to `int32_t` explicitly. Without the casts, intermediate results overflow silently.
- The two compensation functions are marked `__reentrant`. That moves their 32-bit local variables from the scarce direct RAM onto the stack. See [Memory budget](#memory-budget).

### Smoothing

The sensor's IIR filter (coefficient 16) does most of the smoothing. On top of that, `main.c` keeps an exponential running average:

```c
p_sum += p - (p_sum >> 3);    /* p_sum holds 8x the average */
p_avg = p_sum >> 3;
```

Each new reading contributes 1/8. At 4 readings per second the time constant is about 2 s, which is why the lift test needs a few seconds to settle. It needs only one 32-bit variable, where an 8-sample array would cost 32 bytes of RAM.

### Pressure to altitude

The standard-atmosphere formula is

```
h = 44330 × (1 − (P / 101325) ^ 0.1903)
```

with `h` in metres and `P` in Pa. Computing it with SDCC's float `powf` made the firmware about 7.3 KB and overflowed the 8051's direct RAM, so it is precomputed instead.

`tools/gen_alt_table.py` evaluates the formula every 1000 Pa from 50 000 to 110 000 Pa (61 points, from 5575.3 m down to −698.4 m) and prints the values in 0.1 m units. They're pasted into `src/altitude.c` as a `const __code int32_t` table, which keeps them in flash (without `const __code`, SDCC would copy the table into RAM at startup). `altitude_dm()` finds the two neighbouring points and interpolates linearly between them. Pressures outside the table are clamped to the ends.

Interpolation error, from `python3 tools/gen_alt_table.py --check`:

| Pressure | Exact | Table |
|---|---|---|
| 101325 Pa | 0.00 m | 0.0 m |
| 100000 Pa | 110.9 m | 110.9 m |
| 90000 Pa | 988.67 m | 988.7 m |
| 60000 Pa | 4207.13 m | 4207.1 m |

The worst error over the whole range is 0.27 m, at the high-altitude end. Near sea level it's under 0.1 m.

Both modes use the same function:

```c
alt = altitude_dm(p_avg) - altitude_dm(p0);
```

In absolute mode `p0` is 101325 Pa (so the second term is 0). In relative mode `p0` is the smoothed pressure at the moment you pressed the button. Taking the difference of two standard-atmosphere heights instead of using `P/P0` directly in the formula differs by less than 0.1% near sea level.

To change the table range or step, edit `PMIN`, `PMAX` and `STEP` in both `tools/gen_alt_table.py` and `src/altitude.c`, then regenerate:

```sh
python3 tools/gen_alt_table.py --check    # accuracy report
python3 tools/gen_alt_table.py            # C initializer to paste into src/altitude.c
```

### Number formatting

There's no `printf`; SDCC's version is several KB. `lcd_tenths()` in `main.c` prints a fixed-point value in tenths, right-aligned to a given width, with a minus sign when negative. `uart_put_i32()` and `uart_put_hex8()` in `uart.c` do the same job for the serial port.

### I2C

`src/i2c.c` is a single-master bit-bang driver. Each half clock period is a `delay_us(1)` call, which with call overhead comes to several microseconds, so the bus runs at a few tens of kHz (not measured), comfortably inside the sensor's 100/400 kHz limits.

- `i2c_init()` checks whether SDA is stuck low (a sensor left mid-byte by an 8051 reset) and clocks SCL up to 9 times to free it, then sends a START and STOP.
- `i2c_write()` returns 1 if the slave acknowledged.
- `i2c_read(ack)` sends ACK for every byte except the last, which gets NACK.
- `i2c_read_regs()` writes the register address, sends a repeated START, then reads `n` bytes.
- `i2c_probe()` sends just the address byte and reports whether anything acknowledged.

### LCD driver

`src/lcd.c` drives the HD44780 in 4-bit mode. The pins are written one bit at a time (`P2_4 = ...`) instead of writing all of Port 2, so the LED on P2.0 and the contrast PWM on P2.3 are never disturbed.

Initialisation follows the datasheet's "initialisation by instruction" sequence: wait 50 ms, send nibble 0x3 three times (5 ms, 1 ms, 1 ms apart), then 0x2 to switch to 4-bit mode, then 0x28 (4-bit, 2 lines, 5x8 font), 0x0C (display on, cursor off), 0x06 (increment) and 0x01 (clear). Ordinary commands and characters get a 50 µs delay. Clear and home get 2 ms.

### Delays

`delay_ms()` in `src/delay.c` uses Timer0 in 16-bit mode, reloaded with 0xFC66 (922 machine cycles, about 1.000 ms) for each millisecond. It stays accurate even with the contrast interrupt running, because the timer counts in hardware. It changes only the Timer0 half of TMOD, so Timer1's UART setting is left alone.

`delay_us()` is a plain busy loop that always runs longer than requested. It's only used where a minimum delay is needed (LCD writes, I2C).

### UART

`src/uart.c` sets Timer1 to 8-bit auto-reload with TH1 = 0xFD and SMOD = 0, which gives 9600 baud with an 11.0592 MHz crystal (that's why the board uses that odd frequency). Transmission is polled: `uart_putc()` waits for TI before writing SBUF. `TI` is set to 1 at init so the first character doesn't wait forever.

### LCD contrast without a pot

An HD44780 needs V0 slightly above 0 V, usually set with a 10k pot or a 1–2.2k resistor from V0 to GND. None was available. V0 straight to GND made the screen grey all over, readable only at a steep angle, and the 10k and 220k resistors on hand lifted V0 too far and blanked it. (Inside the module, V0 sits at the bottom of a resistor ladder of about 10k from VDD, so a 10k resistor puts V0 near half the supply.)

So V0 is wired to the spare pin P2.3, and `src/contrast.c` switches it rapidly between low and released:

- **Pin low:** V0 = 0 V.
- **Pin released:** V0 floats up through the LCD's internal ladder and the port's weak pull-up.

The LCD responds to the average, so the share of time the pin is released sets the contrast. It's 40% (`CONTRAST_PCT` in `src/main.c`), chosen by eye with the sweep build.

Timer2 runs in 16-bit auto-reload mode with a 256-machine-cycle period, so the PWM runs at 11.0592 MHz / 12 / 256 = 3.6 kHz, fast enough not to flicker. At 40%, the pin is released for 102 cycles and low for 154. The interrupt handler `contrast_isr` is written in assembly and declared `__naked`:

```asm
clr  TF2                   ; acknowledge the interrupt
jbc  contrast_high, 1$     ; was it released? -> go low
setb contrast_high         ; was low -> release the pin
setb P2.3
mov  RCAP2H, contrast_lo_h ; reload value for the phase after this one
mov  RCAP2L, contrast_lo_l
reti
1$:
clr  P2.3
mov  RCAP2H, contrast_hi_h
mov  RCAP2L, contrast_hi_l
reti
```

On each overflow Timer2 reloads itself from RCAP2, which the previous interrupt loaded with the length of the phase that's now starting. The handler flips the pin and loads the length of the phase after that. None of these instructions touch the accumulator or the flags, so nothing has to be saved. Each interrupt costs about 13 machine cycles, two interrupts per period, so the PWM uses roughly 10% of the CPU.

Each phase is held to at least 24 cycles, enough time for the interrupt to run, so the usable range is about 9% to 91%. 0% turns the timer off and holds V0 at 0 V.

It's safe for the board: when low, P2.3 only sinks the ladder current (well under 1 mA), and when released it's an ordinary weak pull-up. V0 always stays between 0 V and VDD.

**Tuning:** if you change the LCD, or the contrast looks wrong, run

```sh
make clean flash EXTRA=-DCONTRAST_SWEEP
```

Line 1 then shows `Contrast NN%` and steps 0, 5, 10 … 60% every 3 s, with a test pattern on line 2. Note the percentage that reads best straight on, set `CONTRAST_PCT` in `src/main.c` to it, and run `make clean flash`.

**Going back to a resistor:** if you fit a 1–2.2k resistor from V0 to GND (or a 10k pot with its wiper on V0), set `CONTRAST_PCT` to 0. That stops Timer2 and the interrupt, and holds P2.3 low, which is harmless with nothing connected to it. Removing `contrast.c` entirely also means removing the `contrast.h` include, the `contrast_init()` call and the sweep code from `main.c`.

**SDCC gotcha:** SDCC generates the interrupt vector table only in the file that contains `main()`, and only for interrupt handlers whose prototypes it can see there. That's why `contrast.h` (included by `main.c`) declares `contrast_isr`. Without the prototype the build succeeds, but nothing is placed at vector 5 (address 0x2B). SDCC puts other library code there, so the first Timer2 interrupt jumps into the middle of unrelated code. Any new interrupt handler needs the same treatment.

## Memory budget

The AT89S52 has 8 KB of flash and 256 bytes of internal RAM. There's no external RAM on this board, so `__xdata`, `__pdata`, `--model-medium` and `--model-large` must not be used. They link fine and then fail at runtime.

Flash is not the tight resource (6402 of 8192 bytes used). RAM is.

| Region | Addresses | Addressing | Used for |
|---|---|---|---|
| Register bank 0 | 0x00–0x07 | direct | R0–R7 |
| Bit-addressable | 0x20–0x2F | direct / bit | `__bit` variables (`contrast_high`) |
| Direct RAM | 0x08–0x7F | direct | globals and the static local variables of every non-reentrant function (`DSEG`) |
| Indirect RAM | 0x80–0xFF (and leftovers below) | `@R0`/`@R1` only | `__idata` variables (`ISEG`) and the stack |

In SDCC's small model, every local variable of an ordinary function is a fixed static slot in direct RAM, and the whole `DSEG` must fit in one contiguous block below 0x80. The first build with float and the 32-bit formulas needed 125 bytes there and failed to link with `Could not get NN consecutive bytes in internal RAM for area DSEG`. These changes fixed it:

| Change | Where |
|---|---|
| Calibration constants in an `__idata` struct | `bme280.c` |
| `t_fine` and the 6-byte raw-data buffer in `__idata` | `bme280.c` |
| `compensate_T` / `compensate_P` marked `__reentrant` so their locals live on the stack | `bme280.c` |
| Formatting buffers in `__idata` | `uart.c`, `main.c` |
| Lookup table instead of `powf` | `altitude.c` |
| One running-average variable instead of a sample array | `main.c` |

`make size` prints the RAM map. The legend: `a`–`z` are each module's direct data, `B` bits, `Q` overlay, `I` `__idata`, `S` stack. The current stack starts at 0x98 with 104 bytes free. Keep at least about 20 bytes of headroom when adding code, and put new buffers in `__idata`.

## Source tour

```
.
├── Makefile                 build, flash, size, clean
├── CLAUDE.md                project notes and hardware rules
├── README.md
├── src/
│   ├── main.c               startup, main loop, button, averaging, display, contrast sweep
│   ├── delay.c/.h           delay_ms (Timer0), delay_us (busy loop)
│   ├── uart.c/.h            9600 8N1 polled output, integer and hex printing
│   ├── lcd.c/.h             HD44780 4-bit driver
│   ├── i2c.c/.h             bit-banged I2C master, register read/write helpers
│   ├── bme280.c/.h          sensor detection, configuration, calibration, Bosch compensation
│   ├── altitude.c/.h        pressure-to-altitude lookup table and interpolation
│   └── contrast.c/.h        Timer2 PWM for LCD contrast on P2.3
└── tools/
    └── gen_alt_table.py     generates and checks the altitude table
```

### Public functions

| Module | Function | Purpose |
|---|---|---|
| delay | `delay_ms(ms)` | accurate millisecond delay on Timer0 |
| delay | `delay_us(us)` | at least `us` microseconds, rough |
| uart | `uart_init()`, `uart_putc()`, `uart_puts()` | setup and output |
| uart | `uart_put_i32()`, `uart_put_hex8()`, `uart_crlf()` | number formatting |
| lcd | `lcd_init()`, `lcd_cmd()`, `lcd_putc()`, `lcd_puts()` | setup and output |
| lcd | `lcd_goto(col, row)`, `lcd_clear()` | cursor position, clear screen |
| i2c | `i2c_init()`, `i2c_start()`, `i2c_stop()`, `i2c_write()`, `i2c_read()` | bus primitives |
| i2c | `i2c_probe(addr)`, `i2c_write_reg()`, `i2c_read_regs()` | 7-bit address helpers, return 1 on success |
| bme280 | `bme280_init()` | returns chip ID (0x60 / 0x58) or 0 if not found |
| bme280 | `bme280_read(&t, &p)` | temperature in 0.01 °C, pressure in Pa, returns 1 on success |
| altitude | `altitude_dm(p)` | standard-atmosphere altitude in 0.1 m |
| contrast | `contrast_init(pct)`, `contrast_set(pct)` | start / change the contrast PWM |

### Tunable constants

| Constant | File | Default | Effect |
|---|---|---|---|
| `POLL_MS` | main.c | 50 | button poll interval |
| `SAMPLE_POLLS` | main.c | 5 | polls per sensor reading (5 × 50 ms = 250 ms) |
| `HOLD_POLLS` | main.c | 40 | hold time for absolute mode (40 × 50 ms = 2 s) |
| `AVG_SHIFT` | main.c | 3 | running average weight 1/2^n; higher is smoother and slower |
| `CONTRAST_PCT` | main.c | 40 | LCD contrast PWM |
| `P0_STD` | main.c | 101325 | absolute-mode reference pressure in Pa |
| `CONFIG_VAL` | bme280.c | 0x30 | sensor standby and IIR filter |
| `CTRL_MEAS_VAL` | bme280.c | 0x57 | sensor oversampling and mode |
| `PERIOD` | contrast.c | 256 | PWM period in machine cycles |

## Accuracy and limitations

- **Relative mode assumes standard-atmosphere temperature.** The table is built for 15 °C air at sea level, where 1 hPa is about 8.3 m. Warmer air is less dense: at 32 °C, 1 hPa is about 8.8 m, so relative heights read roughly 5–6% low. A 1 m lift shows about 0.95 m. Correcting it with the measured temperature would be a small change.
- **Weather drift.** Atmospheric pressure changes by up to a few hPa over hours, and every 0.1 hPa looks like about 0.8 m. A zero set in the morning can drift by several metres by evening. Tap the button again to re-zero.
- **Absolute mode is only as good as the weather.** It assumes 1013.25 hPa at sea level. On a high-pressure day it reads below sea level, as in the −5.5 m test reading. There's no way to enter the local sea-level pressure (QNH).
- **Temperature reads high.** The sensor sits on a breakout near the board and warms up slightly. It's the sensor's own temperature, not a calibrated room temperature.
- **Resolution.** Pressure is computed in whole Pa, about 0.08 m steps. The display shows 0.1 m, and the last digit jitters slightly.
- **Response time.** The sensor's IIR filter plus the running average take 2–3 s to settle after a height change.
- **Table range.** Pressures below 50 kPa (above about 5.5 km) or above 110 kPa clamp to the ends of the table.
- **No persistence.** The zero reference resets on power-off; the AT89S52 has no EEPROM.
- **No sensor recovery.** If the sensor isn't found at startup the firmware stops at `No sensor`. If it disconnects later, the display freezes on the last reading (the LED stops blinking) until reset.
- **Humidity is not used**, even on a real BME280.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| avrdude: `cannot set sck period` | Old programmer firmware | Ignore it |
| avrdude can't find the programmer | Not plugged in, or no udev rule | Check `lsusb` for `16c0:05dc`, install the udev rule |
| `Broken pipe` / `unable to write flash` | USB cable moved during the write | Flash again, don't touch the cable |
| Verify mismatch | Erase failed silently, or ISP clock too fast | Flash again; if it persists, fit the programmer's SCK jumper |
| LCD completely blank | V0 too high, LCD unpowered, or RW floating | Check pins 1, 2 and 5; run the contrast sweep |
| LCD grey all over / only readable at an angle | Contrast too strong | Raise `CONTRAST_PCT` |
| LCD shows faint or no text but powers up | Contrast too weak | Lower `CONTRAST_PCT` |
| Top row solid blocks, no text | LCD powered but not initialised: RS/E/D4–D7 wiring | Check the LCD table above |
| Garbage characters | Data lines swapped, or a loose D4–D7 wire | Check the LCD table above |
| `No sensor` | Sensor not on 0x76/0x77, or SDA/SCL swapped or loose | Check the sensor wiring, then reset |
| Temperature like −140 °C or hundreds of degrees | Calibration read wrong | Should not happen with this code; check for I2C errors on a scan |
| LED not blinking | Firmware stuck or no sensor readings | Reset; check the sensor wiring |
| Button does nothing | JP2 jumper missing | Fit JP2 |
| LED never lights | JP3 jumper missing | Fit JP3 |
| Linker: `Could not get NN consecutive bytes in internal RAM for area DSEG` | Too many globals or locals in direct RAM | Move buffers to `__idata`, mark big 32-bit functions `__reentrant`; see [Memory budget](#memory-budget) |
| Crash or reset as soon as interrupts are enabled | Handler prototype not visible in `main.c`, so it's missing from the vector table | Declare it in a header that `main.c` includes |
| `make EXTRA=...` has no effect | Old objects reused | `make clean` first |

## Development history

The firmware was brought up one stage at a time, each flashed and checked on the board before moving on. Each stage is one commit:

| Commit | Stage | Check on hardware |
|---|---|---|
| `08a1f2e` | Makefile, Timer0 delay, LED blink | LED blinks at about 1 Hz |
| `5268ac4` | UART "hello" | Not checked: no serial adapter |
| `83aae19` | HD44780 4-bit LCD driver | Text and a counter on the LCD (RW and V0 had to be tied to GND first) |
| `e95aa60` | Bit-banged I2C, bus scan, chip ID | `I2C: 76`, `ID 58 BMP280` |
| `644b2b1` | Compensation and table-based altitude | 32.8 °C, 1013.9 hPa, −5.5 m |
| `67869c7` | Zero button, running average, absolute/relative modes | Zero, 1 m lift and 2 s hold all work |
| `7f41e3a` | LCD contrast PWM on P2.3 | Readable straight on at 40%, no flicker |

Problems hit along the way:

- **The LCD showed nothing.** V0 (contrast) and RW were both unconnected. Tying both to GND made text appear, but only at a steep angle.
- **Float `powf` didn't fit.** It pushed flash to about 7.3 KB and overflowed direct RAM, so it was replaced by the lookup table.
- **The linker ran out of direct RAM** even after removing float, because SDCC gives each local variable a static slot. It was fixed with `__idata` and `__reentrant` (see [Memory budget](#memory-budget)).
- **Flashing seemed to hang.** It was just slow: about 26 ms per byte. Short command timeouts killed it partway through.
- **No resistor or pot for contrast,** so it was done in software with Timer2.
- **The sensor is a BMP280,** not the BME280 it was sold as. The code handles both.
