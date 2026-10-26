#include <8052.h>
#include <stdint.h>
#include "delay.h"
#ifdef UART_DEBUG
#include "uart.h"
#endif
#include "lcd.h"
#include "i2c.h"
#include "bme280.h"
#include "altitude.h"

#define LED P2_0
#define BTN P3_3		/* active low, onboard button (JP2) */

#define P0_STD		101325UL	/* Pa, standard sea level */
#define SAMPLE_POLLS	5		/* read the sensor every 5 ticks = 250 ms */
#define HOLD_POLLS	40		/* 2 s hold = back to absolute mode */
#define AVG_SHIFT	3		/* running average over ~8 samples */

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
	uint8_t id, poll = 0, held = 0;
	uint8_t relative = 0;
	int32_t t, alt;
	uint32_t p, p_avg, p0 = P0_STD;
	uint32_t p_sum = 0;		/* (1 << AVG_SHIFT) x average pressure */

#ifdef UART_DEBUG
	uart_init();
#endif
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
	tick_init();

	for (;;) {
		/* press = zero here (relative mode), hold 2 s = absolute mode */
		if (!BTN) {
			if (held < 255)
				held++;
			if (held == 2 && p_sum) {
				p0 = p_sum >> AVG_SHIFT;
				relative = 1;
				poll = 0;	/* show the new zero right away */
			} else if (held == HOLD_POLLS) {
				p0 = P0_STD;
				relative = 0;
				poll = 0;
			}
		} else {
			held = 0;
		}

		if (poll == 0 && bme280_read(&t, &p)) {
			LED = !LED;
			if (!p_sum)
				p_sum = p << AVG_SHIFT;
			p_sum += p - (p_sum >> AVG_SHIFT);
			p_avg = p_sum >> AVG_SHIFT;
			alt = altitude_hyps_dm(p_avg, p0, t);

			lcd_goto(0, 0);
			lcd_puts("Alt:");
			lcd_tenths(alt, 8);
			lcd_puts(" m ");
			lcd_putc(relative ? 'R' : 'A');

			lcd_goto(0, 1);
			lcd_putc('T');
			lcd_tenths(t / 10, 5);
			lcd_puts("C P");
			lcd_tenths(p_avg / 10, 6);
			lcd_putc(' ');

#ifdef UART_DEBUG
			uart_puts("T=");
			uart_put_i32(t);
			uart_puts(" P=");
			uart_put_i32(p);
			uart_puts(" alt=");
			uart_put_i32(alt);
			uart_putc(relative ? 'R' : 'A');
			uart_crlf();
#endif
		}
		if (++poll >= SAMPLE_POLLS)
			poll = 0;
		tick_wait();
	}
}
