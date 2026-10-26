#ifndef DELAY_H
#define DELAY_H

/* 11.0592 MHz crystal, 12 clocks per machine cycle -> 1.085 us per cycle */

void delay_ms(unsigned int ms);   /* Timer0 based, accurate */
void delay_us(unsigned char us);  /* rough busy loop, >= us microseconds */

/* fixed 50 ms main-loop tick on Timer2: the period stays exact however
 * long the work between tick_wait() calls takes (up to 50 ms) */
#define TICK_MS 50
void tick_init(void);
void tick_wait(void);

#endif
