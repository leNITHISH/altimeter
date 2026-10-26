#ifndef ALTITUDE_H
#define ALTITUDE_H

#include <stdint.h>

/* standard-atmosphere altitude for pressure p (Pa), in 0.1 m.
 * relative altitude = altitude_dm(p) - altitude_dm(p0) */
int32_t altitude_dm(uint32_t p);

/* temperature-compensated height of p above p0 (hypsometric equation),
 * in 0.1 m. t = air temperature in 0.01 degC. p, p0 in Pa (or both in the
 * same scaled unit, e.g. 1/16 Pa) */
int32_t altitude_hyps_dm(uint32_t p, uint32_t p0, int32_t t) __reentrant;

#endif
