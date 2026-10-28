# Report assets

Images for the project report, in `assets/`. Code, terminal output and tables are rendered with lipgloss (colours, tables, boxes), glow (the markdown card) and freeze (the window), then given a gloss pass by `gen/gloss.py`. All PNGs are 3x scale on white, so they stay sharp in print.

Where each one comes from:

- **board**: measured or observed on the real board (from the README's test notes and the bring-up log)
- **build**: real output of the toolchain on this machine
- **sim**: host-side simulation or maths from `tools/`. Nothing was logged from the sensor, because there was no serial adapter. Say "simulated" in the caption.
- **illustration**: drawn layout, not a photo

To regenerate:

```
report/make_assets.sh          # code, terminal, tables, diagrams (needs go, freeze, rsvg-convert, Maple Mono NF)
python3 report/gen/charts.py   # charts (plain Python + rsvg-convert)
```

`make_assets.sh` rebuilds the firmware with `make -B` for the build shot. It never flashes the board. The avrdude shot uses the log saved in `gen/logs/`.

## Code (`assets/code/`)

| File | Suggested caption | Source |
|---|---|---|
| `01-i2c-start-stop-write.png` | Bit-banged I2C: start, stop and byte write with ACK | `src/i2c.c` 30-71 |
| `02-i2c-read-regs.png` | Burst register read with a repeated start | `src/i2c.c` 117-134 |
| `03-bme280-init.png` | Sensor probe at 0x76/0x77, chip ID check, calibration read and configuration | `src/bme280.c` 44-67 |
| `04-bme280-compensation.png` | Bosch 32-bit integer compensation for temperature and pressure | `src/bme280.c` 69-105 |
| `05-bme280-read.png` | Reading the 20-bit raw values and compensating them | `src/bme280.c` 107-122 |
| `06-altitude-isa-table.png` | Standard-atmosphere lookup table with linear interpolation | `src/altitude.c` 1-38 |
| `07-altitude-hypsometric.png` | Temperature-corrected (hypsometric) altitude in 32-bit integers | `src/altitude.c` 40-66 |
| `08-kalman-model.png` | Kalman filter model and fixed-point constants | `src/kalman.c` 4-25 |
| `09-kalman-step.png` | One Kalman predict/update step in Q8/Q12 fixed point | `src/kalman.c` 50-74 |
| `10-timer2-tick.png` | Timer2 auto-reload 50 ms loop tick | `src/delay.c` 29-49 |
| `11-lcd-nibble.png` | HD44780 4-bit write path | `src/lcd.c` 12-38 |
| `12-lcd-init.png` | HD44780 4-bit initialisation sequence | `src/lcd.c` 62-82 |
| `13-main-loop.png` | Main loop: buttons every tick, sensor and filter every 250 ms | `src/main.c` 202-259 |
| `14-main-display.png` | Building the two LCD lines and the four line-2 views | `src/main.c` 82-117 |
| `15-button-events.png` | Tap/hold detection for the external button | `src/main.c` 63-80 |
| `16-qnh-editor.png` | QNH editor: digit entry and range check | `src/main.c` 150-177 |
| `17-makefile.png` | Makefile for SDCC, packihx and avrdude | `Makefile` |
| `18-kalman-sim-bitexact.png` | Python copy of the C filter, used to tune and verify it on the PC | `tools/kalman_sim.py` 122-147 |

## Terminal (`assets/terminal/`)

| File | Suggested caption | Source |
|---|---|---|
| `01-make-build.png` | Building the firmware with SDCC | build |
| `02-make-size.png` | Memory report: 8118 of 8192 bytes of flash, 63 bytes of stack left | build |
| `03-avrdude-flash.png` | Flashing over USBasp with avrdude. The "sck period" errors are harmless with the programmer's old firmware. | board, saved log from the 26 Oct flash (an 8113-byte image; the final build is 8118 bytes) |
| `04-kalman-sim.png` | Kalman tuning report from the simulator | sim |
| `05-kalman-consts.png` | Fixed-point constants generated for `kalman.c` | sim |
| `06-check-hyps.png` | Integer hypsometric formula against the exact one, and ISA error vs temperature | sim |
| `07-alt-table.png` | Lookup-table interpolation error | sim |
| `08-git-log.png` | Development history, one commit per bring-up stage | git |

## Tables (`assets/tables/`)

| File | Contents | Source |
|---|---|---|
| `hardware.png` | Parts and toolchain | README |
| `pins.png` | 8051 pin map | README |
| `lcd-wiring.png` | 16-pin LCD header wiring | README |
| `sensor-wiring.png` | Sensor breakout wiring | README |
| `sensor-config.png` | BMP280 registers used and the values written | README, `src/bme280.c` |
| `results.png` | What was tested on the board and what happened | board |
| `not-tested.png` | What couldn't be tested and why | board |
| `history.png` | Stage-by-stage bring-up and the check done on hardware | board, git |
| `flash-budget.png` | Flash use per module | build |
| `flash-cuts.png` | What was tried to fit into 8 KB | build |
| `ram-regions.png` | 8051 internal RAM regions and what uses them | build |
| `kalman-tuning.png` | Noise and lag: raw vs running average vs Kalman | sim |
| `gps-vs-baro.png` | Why phone GPS height and barometric altitude disagree | README |

## Charts (`assets/charts/`)

| File | Suggested caption | Source |
|---|---|---|
| `kalman-lift.png` | Simulated 1 m lift: the Kalman filter follows the step about 1.5 s sooner than the old running average | sim |
| `kalman-speed.png` | Simulated vertical speed during the lift; short moves read low because of smoothing | sim |
| `isa-vs-temperature.png` | Error of the 15 °C standard-atmosphere formula vs air temperature: about 6% low at the measured 32.8 °C | sim (maths) |
| `hypsometric-integer-error.png` | Firmware integer altitude vs the exact formula: within 10 cm up to 300 m | sim (maths) |
| `isa-table-error.png` | Lookup table vs exact ISA formula: within 0.2 m over its range | sim (maths) |
| `flash-by-module.png` | Flash use by module | build |

## Cards (`assets/cards/`)

| File | Contents | Source |
|---|---|---|
| `limitations.png` | Accuracy and limitations, rendered with glow | README |

## Diagrams (`assets/diagrams/`)

| File | Suggested caption | Source |
|---|---|---|
| `block-diagram.png` | System block diagram with the pins used | README pin map |
| `lcd-views.png` | LCD layouts for the four views and the QNH editor | illustration: T/P (32.8 °C, 1013.9 hPa) and 16.5 m are real readings, the rest are example values |
