#ifndef CONTRAST_H
#define CONTRAST_H

#include <stdint.h>

/* LCD contrast without a pot: V0 is wired to P2.3 and PWMed by Timer2.
 * pct = share of each period the pin is released (V0 floats up through
 * the LCD's internal ladder); higher = lighter. 0 = V0 held at GND. */

void contrast_init(uint8_t pct);
void contrast_set(uint8_t pct);

/* must be visible in main.c so sdcc puts it in the vector table */
void contrast_isr(void) __interrupt(5) __naked;

#endif
