#include <8052.h>
#include "delay.h"

#define LED P2_0

void main(void)
{
	for (;;) {
		LED = !LED;
		delay_ms(500);
	}
}
