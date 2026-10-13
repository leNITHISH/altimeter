#include <8052.h>
#include <stdint.h>
#include "delay.h"
#include "uart.h"

#define LED P2_0

void main(void)
{
	int32_t n = 0;

	uart_init();
	for (;;) {
		LED = !LED;
		uart_puts("hello ");
		uart_put_i32(n++);
		uart_crlf();
		delay_ms(500);
	}
}
