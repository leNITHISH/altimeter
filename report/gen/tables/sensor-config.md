| **Register** | **Value** | **Meaning** |
|---|---|---|
| `0xD0` id | `0x58` read | BMP280 (`0x60` would be a BME280) |
| `0xE0` reset | `0xB6` | soft reset, then 5 ms wait |
| `0xF5` config | `0x2C` | standby 62.5 ms, IIR filter x8 |
| `0xF4` ctrl_meas | `0x57` | temp oversampling x2, pressure x16, normal mode |
| `0x88-0x9F` | 24 bytes | calibration `dig_T1..T3`, `dig_P1..P9`, one burst read |
| `0xF7-0xFC` | 6 bytes | raw 20-bit pressure and temperature |
