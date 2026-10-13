#include <8052.h>
#include "delay.h"

/* 1 ms = 921.6 machine cycles; reload 65536 - 922 = 0xFC66 */
#define T0_1MS_H 0xFC
#define T0_1MS_L 0x66

void delay_ms(unsigned int ms)
{
	TMOD = (TMOD & 0xF0) | 0x01;	/* Timer0 mode 1, leave Timer1 (UART) alone */
	while (ms--) {
		TH0 = T0_1MS_H;
		TL0 = T0_1MS_L;
		TF0 = 0;
		TR0 = 1;
		while (!TF0)
			;
		TR0 = 0;
	}
}

void delay_us(unsigned char us)
{
	/* each iteration is a few machine cycles (> 2 us), so this overshoots */
	while (us--)
		;
}
