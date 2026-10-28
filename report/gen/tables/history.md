| **Commit** | **Stage** | **Checked on hardware** |
|---|---|---|
| `08a1f2e` | Makefile, Timer0 delay, LED blink | LED blinks at about 1 Hz |
| `5268ac4` | UART "hello" | not checked: no serial adapter |
| `83aae19` | HD44780 4-bit LCD driver | text and a counter on the LCD |
| `e95aa60` | bit-banged I2C, bus scan, chip ID | `I2C: 76`, `ID 58 BMP280` |
| `644b2b1` | compensation, table-based altitude | 32.8 °C, 1013.9 hPa, -5.5 m |
| `67869c7` | zero button, running average, modes | zero, 1 m lift, 2 s hold all work |
| `7f41e3a` | LCD contrast PWM on P2.3 | readable straight on at 40% duty |
| `a85b940` | PWM removed (pot fitted), Timer2 tick | display fine, updates at 4 Hz |
| `90335b6` | temperature-corrected altitude | 1 m lift reads 1.0-1.1 m |
| `ca5da7c` | Kalman, 2nd button, views, QNH editor | all views, speed and editor work |
