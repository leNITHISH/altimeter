#ifndef DELAY_H
#define DELAY_H

/* 11.0592 MHz crystal, 12 clocks per machine cycle -> 1.085 us per cycle */

void delay_ms(unsigned int ms);   /* Timer0 based, accurate */
void delay_us(unsigned char us);  /* rough busy loop, >= us microseconds */

#endif
