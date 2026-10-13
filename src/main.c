#include <8052.h>
#include <stdint.h>
#include "delay.h"
#include "uart.h"
#include "lcd.h"

#define LED P2_0

void main(void)
{
	uint8_t n = 0;

	uart_init();
	lcd_init();
	lcd_puts("8051 Altimeter");
	uart_puts("lcd ok\r\n");

	for (;;) {
		LED = !LED;
		lcd_goto(0, 1);
		lcd_puts("count ");
		lcd_putc('0' + n / 100);
		lcd_putc('0' + n / 10 % 10);
		lcd_putc('0' + n % 10);
		n++;
		delay_ms(500);
	}
}
