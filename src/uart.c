#include <8052.h>
#include "uart.h"

void uart_init(void)
{
	SCON = 0x50;			/* mode 1, receiver enabled */
	TMOD = (TMOD & 0x0F) | 0x20;	/* Timer1 mode 2 (8-bit auto reload) */
	TH1 = 0xFD;			/* 9600 baud at 11.0592 MHz, SMOD = 0 */
	TL1 = 0xFD;
	PCON &= 0x7F;
	TR1 = 1;
	TI = 1;				/* so the first uart_putc doesn't block */
}

void uart_putc(char c)
{
	while (!TI)
		;
	TI = 0;
	SBUF = c;
}

void uart_puts(const char *s)
{
	while (*s)
		uart_putc(*s++);
}

void uart_put_hex8(uint8_t v)
{
	static const char hex[] = "0123456789ABCDEF";

	uart_putc(hex[v >> 4]);
	uart_putc(hex[v & 0x0F]);
}

void uart_put_i32(int32_t v)
{
	char buf[11];
	uint8_t i = 0;
	uint32_t u;

	if (v < 0) {
		uart_putc('-');
		u = -v;
	} else {
		u = v;
	}
	do {
		buf[i++] = '0' + u % 10;
		u /= 10;
	} while (u);
	while (i)
		uart_putc(buf[--i]);
}

void uart_crlf(void)
{
	uart_putc('\r');
	uart_putc('\n');
}
