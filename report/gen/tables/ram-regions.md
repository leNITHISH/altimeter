| **Region** | **Addresses** | **Used for** |
|---|---|---|
| Register bank 0 | `0x00-0x07` | R0-R7 |
| Bit-addressable | `0x20-0x2F` | `__bit` variables |
| Direct RAM | `0x08-0x7F` | globals + locals of non-reentrant functions |
| Indirect RAM | `0x80-0xFF` | `__idata` buffers and the stack (63 B free) |
