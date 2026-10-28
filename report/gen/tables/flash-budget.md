| **Part** | **Bytes** |
|---|---|
| `bme280.c` (compensation) | 2549 |
| `main.c` (loop, buttons, views, QNH editor) | 2093 |
| `altitude.c` (both formulas + 42-byte table) | 897 |
| runtime library (32-bit mul/div) + startup | 878 |
| `kalman.c` | 874 |
| `i2c.c` | 500 |
| `lcd.c` | 249 |
| `delay.c` | 78 |
| **total** | **8118 of 8192** |
