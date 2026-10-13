#ifndef ALTITUDE_H
#define ALTITUDE_H

#include <stdint.h>

/* standard-atmosphere altitude for pressure p (Pa), in 0.1 m.
 * relative altitude = altitude_dm(p) - altitude_dm(p0) */
int32_t altitude_dm(uint32_t p);

#endif
