| **Change** | **Effect** |
|---|---|
| `--opt-code-size` | 0 bytes |
| UART prints moved to a debug build | -386 bytes |
| one shared Q12 multiply helper in the Kalman code | about -110 bytes |
| table as `uint16_t` + offset instead of `int32_t` | about -80 bytes |
| table step 1024 Pa (shifts, no division), shorter range | about -110 bytes |
| compensation not `__reentrant` | -164 bytes, but needs 44 B more direct RAM: reverted |
| hypsometric function not `__reentrant` | -92 bytes, costs 20 B direct RAM: reverted |
