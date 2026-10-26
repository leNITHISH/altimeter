#include <stdint.h>
#include "kalman.h"

/* State x = [p, v]: pressure and its rate of change. Every 0.25 s:
 *
 *   predict  p += v dt                  F = [1 dt; 0 1]
 *            P  = F P F' + Q            Q: random acceleration
 *   update   S  = P00 + R               R: measurement noise
 *            K0 = P00 / S, K1 = P01 / S
 *            y  = z - p                 innovation
 *            p += K0 y, v += K1 y
 *            P  = (I - K H) P           H = [1 0]
 *
 * Working on pressure, not altitude, means zeroing or a new QNH never
 * disturbs the filter. Fixed point: p, v and P in Q8, gains in Q12,
 * dt = 1/4 as shifts. tools/kalman_sim.py has a float reference and a
 * bit-exact copy of this code; tuning and the constants come from there
 * (sigma_z = 0.71 Pa, random acceleration 0.1 m/s^2). */
#define KF_R		129L	/* 0.504 Pa^2 */
#define KF_Q00		0L	/* 0.0013 */
#define KF_Q01		3L	/* 0.0100 */
#define KF_Q11		21L	/* 0.0803 */
#define KF_P00_INIT	25600L	/* 100 Pa^2: don't trust the first guess */
#define KF_P11_INIT	25600L
#define Y_MAX		(100L * 256)	/* clamp the innovation to +/-100 Pa */

__idata int32_t kf_p, kf_v;
static __idata int32_t P00, P01, P11;

/* (a * b) >> 12 and (a << 12) / b, shared to save flash */
static int32_t mul_q12(int32_t a, int32_t b)
{
	return (a * b) >> 12;
}

static int32_t div_q12(int32_t a, int32_t b)
{
	return (a << 12) / b;
}

void kalman_init(uint32_t z)
{
	kf_p = (int32_t)z << 8;
	kf_v = 0;
	P00 = KF_P00_INIT;
	P01 = 0;
	P11 = KF_P11_INIT;
}

void kalman_step(uint32_t z)
{
	int32_t s, k0, k1, y;

	/* predict, dt = 1/4 s */
	kf_p += kf_v >> 2;
	P00 += (P01 >> 1) + (P11 >> 4) + KF_Q00;
	P01 += (P11 >> 2) + KF_Q01;
	P11 += KF_Q11;

	/* update */
	s = P00 + KF_R;
	k0 = div_q12(P00, s);
	k1 = div_q12(P01, s);
	y = ((int32_t)z << 8) - kf_p;
	if (y > Y_MAX)
		y = Y_MAX;
	else if (y < -Y_MAX)
		y = -Y_MAX;
	kf_p += mul_q12(k0, y);
	kf_v += mul_q12(k1, y);
	P11 -= mul_q12(k1, P01);
	P01 -= mul_q12(k0, P01);
	P00 -= mul_q12(k0, P00);
}
