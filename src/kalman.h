#ifndef KALMAN_H
#define KALMAN_H

#include <stdint.h>

/* 2-state Kalman filter on pressure, constant-velocity model, one step
 * every 0.25 s. Estimates in Q8 fixed point (value * 256). */
extern __idata int32_t kf_p;	/* pressure, Pa * 256 */
extern __idata int32_t kf_v;	/* rate of change, Pa/s * 256 */

void kalman_init(uint32_t z);	/* z = first measured pressure, Pa */
void kalman_step(uint32_t z);	/* predict 0.25 s ahead, then correct with z */

#endif
