#include <8052.h>
#include "lcd.h"
#include "delay.h"

#define LCD_RS P2_1
#define LCD_E  P2_2
#define LCD_D4 P2_4
#define LCD_D5 P2_5
#define LCD_D6 P2_6
#define LCD_D7 P2_7

/* bit writes so P2.0 (LED) and P2.3 are left untouched */
static void lcd_nibble(uint8_t n)
{
	LCD_D4 = n & 0x01;
	LCD_D5 = (n >> 1) & 0x01;
	LCD_D6 = (n >> 2) & 0x01;
	LCD_D7 = (n >> 3) & 0x01;
	LCD_E = 1;
	__asm__("nop");
	__asm__("nop");
	LCD_E = 0;
}

static void lcd_write(uint8_t v)
{
	lcd_nibble(v >> 4);
	lcd_nibble(v & 0x0F);
	delay_us(50);
}

void lcd_cmd(uint8_t c)
{
	LCD_RS = 0;
	lcd_write(c);
	if (c <= 0x03)		/* clear / home need 1.52 ms */
		delay_ms(2);
}

void lcd_putc(char c)
{
	LCD_RS = 1;
	lcd_write(c);
}

void lcd_puts(const char *s)
{
	while (*s)
		lcd_putc(*s++);
}

void lcd_goto(uint8_t col, uint8_t row)
{
	lcd_cmd(0x80 | (row ? 0x40 : 0x00) | col);
}

void lcd_clear(void)
{
	lcd_cmd(0x01);
}

void lcd_init(void)
{
	LCD_E = 0;
	LCD_RS = 0;
	delay_ms(50);		/* power-on wait */

	/* reset into 8-bit mode three times, then switch to 4-bit */
	lcd_nibble(0x03);
	delay_ms(5);
	lcd_nibble(0x03);
	delay_ms(1);
	lcd_nibble(0x03);
	delay_ms(1);
	lcd_nibble(0x02);
	delay_ms(1);

	lcd_cmd(0x28);		/* 4-bit, 2 lines, 5x8 font */
	lcd_cmd(0x0C);		/* display on, cursor off */
	lcd_cmd(0x06);		/* increment, no shift */
	lcd_clear();
}
