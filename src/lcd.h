#ifndef LCD_H
#define LCD_H

#include <stdint.h>

/* HD44780 16x2, 4-bit mode, RW tied to GND (delays instead of busy flag) */

void lcd_init(void);
void lcd_cmd(uint8_t c);
void lcd_putc(char c);
void lcd_puts(const char *s);
void lcd_goto(uint8_t col, uint8_t row);
void lcd_clear(void);

#endif
