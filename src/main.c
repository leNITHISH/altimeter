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
#include "kalman.h"

#define LED  P2_0
#define BTN  P3_3		/* onboard (JP2): tap = zero, hold 2 s = absolute */
#define BTN2 P3_2		/* external, to GND: tap = next view, hold 1 s = QNH */

#define P0_STD		101325UL	/* Pa, standard sea level */
#define SAMPLE_TICKS	5		/* read the sensor every 5 ticks = 250 ms */
#define TAP_TICKS	2		/* >= 100 ms counts as a press */
#define HOLD_ABS_TICKS	40		/* onboard 2 s hold = absolute mode */
#define HOLD2_TICKS	20		/* external 1 s hold */
#define QNH_MIN		8700		/* 0.1 hPa, lowest sea-level pressure on record ~870 */
#define QNH_MAX		10850		/* highest ~1084 */

#define LCD_CURSOR_ON	0x0E		/* display on, underline cursor */
#define LCD_CURSOR_OFF	0x0C

enum { VIEW_TP, VIEW_ISA, VIEW_VARIO, VIEW_QNH, NVIEWS };
enum { EV_NONE, EV_TAP, EV_HOLD };

static uint8_t relative;		/* 0 = absolute (p0 = QNH), 1 = zeroed */
static uint8_t view;
static uint32_t qnh = P0_STD;		/* Pa, sea-level pressure for absolute mode */
static uint32_t p0_q2 = P0_STD * 4;	/* reference pressure, Pa * 4 */

/* QNH editor: five digits of the value in 0.1 hPa, e.g. 1013.2 */
static uint8_t editing, edit_pos;
static __idata uint8_t digits[5];

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

/* external button: a tap is reported on release, a hold once after 1 s */
static uint8_t btn2_event(void)
{
	static uint8_t held2;
	uint8_t ev = EV_NONE;

	if (!BTN2) {
		if (held2 < 255)
			held2++;
		if (held2 == HOLD2_TICKS)
			ev = EV_HOLD;
	} else {
		if (held2 >= TAP_TICKS && held2 < HOLD2_TICKS)
			ev = EV_TAP;
		held2 = 0;
	}
	return ev;
}

static void show(int32_t t)
{
	uint32_t p_q2 = kf_p >> 6;	/* Kalman pressure, Pa * 4 */

	lcd_goto(0, 0);
	lcd_puts("Alt:");
	lcd_tenths(altitude_hyps_dm(p_q2, p0_q2, t), 8);
	lcd_puts(" m ");
	lcd_putc(relative ? 'R' : 'A');

	lcd_goto(0, 1);
	switch (view) {
	case VIEW_TP:
		lcd_putc('T');
		lcd_tenths(t / 10, 5);
		lcd_puts("C P");
		lcd_tenths((kf_p >> 8) / 10, 6);
		lcd_putc(' ');
		break;
	case VIEW_ISA:		/* same reference, 15 degC standard atmosphere */
		lcd_puts("ISA:");
		lcd_tenths(altitude_dm(p_q2 >> 2) - altitude_dm(p0_q2 >> 2), 8);
		lcd_puts(" m  ");
		break;
	case VIEW_VARIO:	/* height gained in the next second at rate v */
		lcd_puts("V:  ");
		lcd_tenths(altitude_hyps_dm(p_q2 + (kf_v >> 6), p_q2, t), 6);
		lcd_puts(" m/s  ");
		break;
	default:
		lcd_puts("QNH ");
		lcd_tenths(qnh / 10, 6);
		lcd_puts(" hPa  ");
		break;
	}
}

static void editor_draw(uint8_t bad)
{
	uint8_t i;

	lcd_goto(0, 0);
	lcd_puts(bad ? "Only 870-1085!  " : "Set QNH (hPa)   ");
	lcd_goto(0, 1);
	for (i = 0; i < 5; i++) {
		if (i == 4)
			lcd_putc('.');
		lcd_putc('0' + digits[i]);
	}
	lcd_puts("          ");
	lcd_goto(edit_pos < 4 ? edit_pos : 5, 1);	/* cursor under the digit */
}

static void editor_open(void)
{
	uint16_t v = qnh / 10;
	uint8_t i;

	for (i = 5; i--; ) {
		digits[i] = v % 10;
		v /= 10;
	}
	edit_pos = 0;
	editing = 1;
	editor_draw(0);
	lcd_cmd(LCD_CURSOR_ON);
}

/* tap = next value for this digit, hold = next digit / save on the last */
static void editor_event(uint8_t ev)
{
	uint16_t v;
	uint8_t i;

	if (ev == EV_TAP) {
		digits[edit_pos] = (digits[edit_pos] + 1) % 10;
	} else if (edit_pos < 4) {
		edit_pos++;
	} else {
		v = 0;
		for (i = 0; i < 5; i++)
			v = v * 10 + digits[i];
		edit_pos = 0;
		if (v < QNH_MIN || v > QNH_MAX) {
			editor_draw(1);
			return;
		}
		qnh = (uint32_t)v * 10;
		p0_q2 = qnh * 4;
		relative = 0;
		editing = 0;
		lcd_cmd(LCD_CURSOR_OFF);
		return;
	}
	editor_draw(0);
}

void main(void)
{
	uint8_t id, tick = 0, held = 0, ev;
	uint8_t have_kf = 0, redraw = 0;
	int32_t t = 0;
	uint32_t p;

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
	delay_ms(200);		/* first filtered sample */
	tick_init();

	for (;;) {
		/* onboard: press = zero here (relative), hold 2 s = absolute */
		if (!BTN) {
			if (held < 255)
				held++;
			if (held == TAP_TICKS && have_kf) {
				p0_q2 = kf_p >> 6;
				relative = 1;
				redraw = 1;
			} else if (held == HOLD_ABS_TICKS) {
				p0_q2 = qnh * 4;
				relative = 0;
				redraw = 1;
			}
		} else {
			held = 0;
		}

		ev = btn2_event();
		if (ev != EV_NONE) {
			if (editing) {
				editor_event(ev);
			} else if (ev == EV_TAP) {
				view = (view + 1) % NVIEWS;
				redraw = 1;
			} else {
				editor_open();
			}
		}

		/* fixed 0.25 s sample period: the Kalman model depends on it */
		if (tick == 0 && bme280_read(&t, &p)) {
			LED = !LED;
			if (have_kf) {
				kalman_step(p);
			} else {
				kalman_init(p);
				have_kf = 1;
			}
			redraw = 1;
#ifdef UART_DEBUG
			uart_puts("T=");
			uart_put_i32(t);
			uart_puts(" P=");
			uart_put_i32(p);
			uart_puts(" Pk=");
			uart_put_i32(kf_p >> 8);
			uart_crlf();
#endif
		}
		if (redraw && have_kf && !editing) {
			show(t);
			redraw = 0;
		}
		if (++tick >= SAMPLE_TICKS)
			tick = 0;
		tick_wait();
	}
}
