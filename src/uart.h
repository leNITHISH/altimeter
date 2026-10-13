#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);		/* 9600 8N1, Timer1 mode 2 */
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_hex8(uint8_t v);
void uart_put_i32(int32_t v);
void uart_crlf(void);

#endif
