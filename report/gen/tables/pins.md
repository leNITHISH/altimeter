| **Signal** | **8051 pin** | **Notes** |
|---|---|---|
| I2C SCL | `P1.0` | bit-banged, release = write 1 |
| I2C SDA | `P1.1` | bit-banged, read only after writing 1 |
| LCD RS | `P2.1` | |
| LCD E | `P2.2` | |
| LCD D4..D7 | `P2.4..P2.7` | 4-bit mode, RW tied to GND |
| Status LED | `P2.0` | onboard, toggles on every sensor reading |
| Zero button | `P3.3` | onboard: tap = zero, hold 2 s = absolute |
| View / QNH button | `P3.2` | external to GND: tap = next view, hold 1 s = QNH |
| UART TX / RX | `P3.1 / P3.0` | onboard MAX232, 9600 8N1, debug build only |
