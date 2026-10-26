# 8051 Altimeter (MPMC project)

BME280 pressure sensor → AT89S52 → 16x2 HD44780 LCD. Shows altitude (and temp/pressure).

## Hardware (fixed, do not change pin map without asking)

- Board: MyTechnoCare "My 8051" (MY010V2.1), AT89S52 (8 KB flash, 256 B RAM), 11.0592 MHz crystal
- Board is borrowed: never suggest anything that risks damaging it
- Onboard: green LED on P2.0 (jumper JP3), push button on P3.3 / INT1 (jumper JP2), MAX232 on P3.0/P3.1

| Signal        | 8051 pin | Notes |
|---------------|----------|-------|
| I2C SCL       | P1.0     | bit-banged, release = write 1 |
| I2C SDA       | P1.1     | bit-banged, read only after writing 1 |
| LCD RS        | P2.1     | |
| LCD E         | P2.2     | |
| LCD D4..D7    | P2.4..P2.7 | 4-bit mode, RW tied to GND (no busy-flag reads, use delays) |
| Status LED    | P2.0     | active low on this board (verify), heartbeat |
| Zero button   | P3.3     | onboard: tap = set current altitude as 0 m reference, hold 2 s = absolute. Don't change this behaviour |
| View/QNH button | P3.2   | external button to GND: tap = next line-2 view, hold 1 s = QNH editor |
| UART TX/RX    | P3.1/P3.0| debug prints, 9600 8N1 (Timer1 mode 2, TH1 = 0xFD, SMOD=0) |

- BME280 module: 5 V version with onboard regulator + level shifter, powered from board 5 V
- I2C address 0x76 (SDO to GND) or 0x77; firmware should probe both
- Read chip ID at 0xD0 at startup: 0x60 = BME280, 0x58 = BMP280 (common fake/mislabel). Support both; humidity is optional.
- BME280 never clock-stretches, so a simple single-master bit-bang is fine. Port 1 is quasi-bidirectional with internal pull-ups; never drive lines high in a way that fights the slave.
- Avoid Port 0 entirely (open drain, no pull-ups fitted).

## Toolchain (Arch Linux)

- Compiler: `sdcc` (pacman), target `-mmcs51 --model-small`, code size `--code-size 8192`, `--iram-size 256`
- Convert: `packihx` → `.hex`
- Flash: MyTechnoCare 2-in-1 USB ISP programmer (USBasp clone, previously used from Windows/Keil). Linux command: `avrdude -p 89S52 -c usbasp -e -U flash:w:build/main.hex:i` (installed avrdude has `89S52` built in; `8052` is NOT a valid part name, no custom conf needed). Expected signature 0x1e5206. udev rule is already installed at /etc/udev/rules.d/70-usbasp.rules. VERIFIED WORKING: signature reads correctly. avrdude always prints "Error: cannot set sck period; please check for usbasp firmware update" — this is harmless (old programmer firmware), ignore it and do NOT pass -B or try to update programmer firmware. Programmer's SCK jumper is off; if verify fails, ask the user to fit it. Known USBasp quirk: chip erase sometimes fails silently on 89S52, so always verify after write. Writing is slow (~26 ms/byte, ~3.5 min for the current 8.1 KB image): never wrap `make flash` in a short timeout, and do not touch the cable while it runs. Add a udev rule so flashing works without sudo.
- Pin wiring for the LCD is done by the user (RS, E, D4-D7 as in the table, RW to GND, D0-D3 unconnected). Contrast: 10k pot on V0. P2.3 is free (it used to drive V0 by Timer2 PWM; see git history before a85b940). Any future ISR must have its prototype visible in main.c, or sdcc leaves it out of the vector table.
- Provide a `Makefile` with `all`, `flash`, `clean`, and `size` (report flash/RAM usage) targets.

## Firmware design

- Files: `main.c`, `i2c.c/.h`, `lcd.c/.h`, `bme280.c/.h`, `uart.c/.h` (only linked with `EXTRA=-DUART_DEBUG`), `delay.c/.h` (also the Timer2 50 ms loop tick), `altitude.c/.h`, `kalman.c/.h`. Host tools in `tools/`: `gen_alt_table.py`, `check_hyps.py`, `kalman_sim.py` (bit-exact copies of the C integer maths; rerun them after changing it). Slides: `deck/slides.html` → `deck/altimeter-final-review.pdf` via headless chromium.
- Flash is essentially full: 8118 of 8192 bytes. Measure with `make size` before adding anything; cheapest cuts are the QNH view (~60 B) and a steady-state Kalman (~650 B, user would need to agree). Stack has 63 B free.
- Fitted sensor is actually a BMP280 (ID 0x58) at 0x76. Direct RAM (0x08-0x7F) is as tight as flash: put buffers/structs in `__idata` and mark 32-bit-heavy internal functions `__reentrant`. No external RAM, so never use `__xdata` or medium/large models.
- Use Bosch's 32-bit integer compensation formulas (datasheet section 4.2.3 / 8.2); avoid 64-bit math, too heavy for 8051.
- Altitude (line 1): hypsometric equation with the measured temperature, `h = 29.27 * T * ln(p0/p)` with ln approximated by 2(p0-p)/(p0+p), in 32-bit integers (`altitude_hyps_dm`). The standard-atmosphere formula `h = 44330 * (1 - (P/P0)^0.1903)` is a 21-entry uint16 lookup table (1024 Pa steps, ~-430 m to +1.3 km), used only for the ISA comparison view. No float.
- Filtering: 2-state Kalman filter (pressure + rate) in Q8/Q12 fixed point, one step every exactly 250 ms (5 Timer2 ticks). Never trigger extra sensor reads from button handlers; the filter assumes a fixed dt. Tuning constants come from `tools/kalman_sim.py --consts`.
- Two modes: absolute (P0 = QNH, default 1013.25 hPa, set with the P3.2 editor, 870-1085 hPa, lost at power-off) and relative (P0 = pressure captured when P3.3 is pressed). Relative is the main demo mode since it's near sea level here.
- Sensor config: normal mode, pressure oversampling x16, IIR filter x8 (lowered from x16 so the Kalman filter isn't held back by sensor-side lag), temp x2, standby 62.5 ms.
- LCD layout: line 1 `Alt:     1.0 m R` (R/A mode letter). Line 2 cycles with the P3.2 button: `T 32.8C P1013.9`, `ISA:     0.9 m`, `V:     0.3 m/s`, `QNH 1013.2 hPa` (all 16 chars).
- Known-good fallback firmware: git tag `demo-v1` (stage 6 + PWM contrast).
- Keep all buffers small; watch RAM (stack lives in IRAM).

## Bring-up order

1. LED blink (proves toolchain + flashing)
2. UART "hello" (proves clock/baud)
3. LCD text
4. I2C scan + chip ID over UART and LCD
5. Raw ADC values → compensated T/P → altitude
6. Zero button + averaging
