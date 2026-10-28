| **Feature** | **Result on the board** |
|---|---|
| LED heartbeat on P2.0 | works |
| LCD, 4-bit mode, 10k contrast pot | works, readable straight on |
| I2C scan | sensor found at `0x76` |
| Chip ID | `0x58`, a BMP280 (sold as a BME280) |
| Compensated T and P | 32.8 °C, 1013.9 hPa in a warm room |
| Onboard button (P3.3) | tap zeroes, hold 2 s returns to absolute mode |
| External button (P3.2) | tap cycles the 4 views, hold 1 s opens the QNH editor |
| Lift test | about +1.0 m when raised 1 m after zeroing, back to about 0 |
| ISA view on the same lift | slightly lower than line 1 |
| Vertical speed | positive going up, about 0 at rest, negative going down |
| QNH editor | saves valid values; `0800.0` is rejected with `Only 870-1085!` |
