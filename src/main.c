#include <8052.h>
#include <stdint.h>
#include "delay.h"
#include "uart.h"
#include "lcd.h"
#include "i2c.h"
#include "bme280.h"
#include "altitude.h"

#define LED P2_0

#define P0_STD 101325UL		/* Pa, standard sea level */

/* print v (in tenths) right-aligned in width chars, e.g. 1008.4 */
static void lcd_tenths(int32_t v, uint8_t width)
{
	__idata char buf[9];
	uint8_t i = sizeof(buf);
	uint8_t neg = v < 0;
	uint32_t u = neg ? -v : v;

	buf[--i] = '0' + u % 10;
	u /= 10;
	buf[--i] = '.';
	do {
		buf[--i] = '0' + u % 10;
		u /= 10;
	} while (u && i);
	if (neg && i)
		buf[--i] = '-';
	while (sizeof(buf) - i < width)
		lcd_putc(' '), width--;
	while (i < sizeof(buf))
		lcd_putc(buf[i++]);
}

void main(void)
{
	uint8_t id;
	int32_t t, alt;
	uint32_t p;

	uart_init();
	lcd_init();
	i2c_init();

	id = bme280_init();
	if (!id) {
		lcd_puts("No sensor");
		for (;;)
			;
	}
	lcd_puts(id == BME280_ID ? "BME280" : "BMP280");
	delay_ms(200);		/* first IIR-filtered sample */

	for (;;) {
		LED = !LED;
		if (bme280_read(&t, &p)) {
			alt = altitude_dm(p) - altitude_dm(P0_STD);

			lcd_goto(0, 0);
			lcd_puts("Alt:");
			lcd_tenths(alt, 8);
			lcd_puts(" m  ");

			lcd_goto(0, 1);
			lcd_putc('T');
			lcd_tenths(t / 10, 5);
			lcd_puts("C P");
			lcd_tenths(p / 10, 6);
			lcd_putc(' ');

			uart_puts("T=");
			uart_put_i32(t);
			uart_puts(" P=");
			uart_put_i32(p);
			uart_puts(" alt=");
			uart_put_i32(alt);
			uart_crlf();
		}
		delay_ms(250);
	}
}
