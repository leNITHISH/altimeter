#include <8052.h>
#include "contrast.h"

#define LCD_V0		P2_3
#define PERIOD		256	/* machine cycles, ~3.6 kHz at 11.0592 MHz */
#define MIN_PHASE	24	/* ISR entry + body must fit in a phase */

/* referenced by name from the ISR asm, so not static */
__bit contrast_high;
uint8_t contrast_lo_h, contrast_lo_l;	/* Timer2 reload for the low phase */
uint8_t contrast_hi_h, contrast_hi_l;	/* ... and for the released phase */

/* On overflow Timer2 reloads from RCAP2, which the previous ISR loaded
 * with the length of the phase starting now; this ISR flips the pin and
 * loads the length of the phase after that. None of these instructions
 * touch PSW or ACC, so nothing needs saving. */
void contrast_isr(void) __interrupt(5) __naked
{
	__asm
	clr	_TF2
	jbc	_contrast_high, 00001$
	setb	_contrast_high
	setb	_P2_3
	mov	_RCAP2H, _contrast_lo_h
	mov	_RCAP2L, _contrast_lo_l
	reti
00001$:
	clr	_P2_3
	mov	_RCAP2H, _contrast_hi_h
	mov	_RCAP2L, _contrast_hi_l
	reti
	__endasm;
}

void contrast_set(uint8_t pct)
{
	uint16_t hi, lo;

	TR2 = 0;
	ET2 = 0;
	LCD_V0 = 0;
	contrast_high = 0;
	if (pct == 0)
		return;

	hi = (uint16_t)PERIOD * pct / 100;
	if (hi < MIN_PHASE)
		hi = MIN_PHASE;
	if (hi > PERIOD - MIN_PHASE)
		hi = PERIOD - MIN_PHASE;
	lo = PERIOD - hi;
	hi = -hi;			/* 16-bit up-counter reload values */
	lo = -lo;
	contrast_hi_h = hi >> 8;
	contrast_hi_l = hi;
	contrast_lo_h = lo >> 8;
	contrast_lo_l = lo;

	/* start in the low phase; the first reload is the released phase */
	TH2 = contrast_lo_h;
	TL2 = contrast_lo_l;
	RCAP2H = contrast_hi_h;
	RCAP2L = contrast_hi_l;
	TF2 = 0;
	ET2 = 1;
	EA = 1;
	TR2 = 1;
}

void contrast_init(uint8_t pct)
{
	T2CON = 0;			/* 16-bit auto-reload, timer mode */
	contrast_set(pct);
}
