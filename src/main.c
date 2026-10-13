#include <8052.h>
#include <stdint.h>
#include "delay.h"
#include "uart.h"
#include "lcd.h"
#include "i2c.h"

#define LED P2_0

static void lcd_hex8(uint8_t v)
{
	uint8_t n = v >> 4;

	lcd_putc(n < 10 ? '0' + n : 'A' - 10 + n);
	n = v & 0x0F;
	lcd_putc(n < 10 ? '0' + n : 'A' - 10 + n);
}

void main(void)
{
	uint8_t a, found = 0, sensor = 0, id = 0;

	uart_init();
	lcd_init();
	i2c_init();

	/* line 1: every address that ACKs */
	lcd_puts("I2C:");
	uart_puts("i2c scan:");
	for (a = 0x08; a < 0x78; a++) {
		if (i2c_probe(a)) {
			if (found < 4) {
				lcd_putc(' ');
				lcd_hex8(a);
			}
			uart_putc(' ');
			uart_put_hex8(a);
			found++;
			if (a == 0x76 || a == 0x77)
				sensor = a;
		}
	}
	if (!found)
		lcd_puts(" none");
	uart_crlf();

	/* line 2: chip ID of the pressure sensor */
	lcd_goto(0, 1);
	if (!sensor) {
		lcd_puts("no 76/77");
	} else if (!i2c_read_regs(sensor, 0xD0, &id, 1)) {
		lcd_puts("ID read fail");
	} else {
		lcd_puts("ID ");
		lcd_hex8(id);
		lcd_puts(id == 0x60 ? " BME280" : id == 0x58 ? " BMP280" : " ???");
		uart_puts("chip id ");
		uart_put_hex8(id);
		uart_crlf();
	}

	for (;;) {
		LED = !LED;
		delay_ms(500);
	}
}
