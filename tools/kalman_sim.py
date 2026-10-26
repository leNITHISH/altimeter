#!/usr/bin/env python3
"""Simulate the altimeter's 2-state Kalman filter (src/kalman.c).

Model (constant velocity, sampled every DT = 0.25 s):
    state      x = [p, v]       pressure (Pa) and its rate dp/dt (Pa/s)
    predict    p += v * DT
               P = F P F^T + Q,  F = [[1, DT], [0, 1]]
               Q = piecewise-constant white acceleration, sigma A_PA
    update     z = measured pressure (Pa)
               S = P00 + R, K0 = P00 / S, K1 = P01 / S
               y = z - p;  p += K0 y;  v += K1 y
               P00 -= K0 P00;  P01 -= K0 P01;  P11 -= K1 P01

The 8051 runs it in 32-bit integers:
    p, v      Q8   (value * 256)
    P00..P11  Q8
    K0, K1    Q12  (value * 4096)
    DT = 1/4  -> right shifts
This file contains a float reference and a bit-exact emulation of the C
integer code, runs both on a synthetic 1 m lift, and reports noise,
lag, the steady-state gains and the fixed-vs-float difference.

    python3 tools/kalman_sim.py           # report
    python3 tools/kalman_sim.py --consts  # print the Q8 constants for C
"""
import math
import random
import sys

DT = 0.25            # s, 5 ticks of 50 ms
M_PER_PA = 0.0882    # near sea level at ~32 degC (29.27 * 305 / 101300)

# tuning
SIGMA_Z = 0.71       # Pa, noise the filter assumes on each sample
A_MPS2 = 0.1         # m/s^2, how hard we expect it to be moved by hand

# sensor model for the simulation: raw samples at ~9.5 Hz with
# SIGMA_RAW Pa noise, the BMP280's IIR filter (coefficient 16), then the
# 1 Pa rounding of the 32-bit compensation, read every DT
ODR_HZ = 9.5
SIGMA_RAW = 2.5
IIR_COEF = 8         # matches CONFIG_VAL in src/bme280.c

for arg in sys.argv[1:]:                     # e.g. A_MPS2=0.2 SIGMA_Z=0.7
    if "=" in arg:
        k, v = arg.split("=")
        globals()[k] = float(v)
A_PA = A_MPS2 / M_PER_PA          # Pa/s^2
P00_INIT = 100.0     # Pa^2, large: "don't trust the first estimate"
P11_INIT = 100.0     # (Pa/s)^2

def derived():
    global R, Q00, Q01, Q11, R_Q8, Q00_Q8, Q01_Q8, Q11_Q8, A_PA
    global P00_INIT_Q8, P11_INIT_Q8
    A_PA = A_MPS2 / M_PER_PA
    R = SIGMA_Z ** 2
    Q00 = DT ** 4 / 4 * A_PA ** 2
    Q01 = DT ** 3 / 2 * A_PA ** 2
    Q11 = DT ** 2 * A_PA ** 2
    R_Q8, Q00_Q8, Q01_Q8, Q11_Q8 = q8(R), q8(Q00), q8(Q01), q8(Q11)
    P00_INIT_Q8, P11_INIT_Q8 = q8(P00_INIT), q8(P11_INIT)


R = SIGMA_Z ** 2
Q00 = DT ** 4 / 4 * A_PA ** 2
Q01 = DT ** 3 / 2 * A_PA ** 2
Q11 = DT ** 2 * A_PA ** 2

# fixed point
F = 256              # Q8
KF = 4096            # Q12
Y_MAX = 100 * F      # clamp the innovation to +/-100 Pa


def q8(x):
    return int(round(x * F))


R_Q8, Q00_Q8, Q01_Q8, Q11_Q8 = q8(R), q8(Q00), q8(Q01), q8(Q11)
P00_INIT_Q8, P11_INIT_Q8 = q8(P00_INIT), q8(P11_INIT)

INT32 = (-2**31, 2**31 - 1)


def chk(x):
    assert INT32[0] <= x <= INT32[1], f"int32 overflow: {x}"
    return x


def cdiv(a, b):
    """C99 integer division (truncates toward zero)."""
    q = abs(a) // abs(b)
    return chk(q if (a >= 0) == (b >= 0) else -q)


def asr(a, n):
    """sdcc's >> on a signed long: arithmetic shift (rounds toward -inf)."""
    return chk(a >> n)


class KalmanFloat:
    def __init__(self, z):
        self.p, self.v = float(z), 0.0
        self.P00, self.P01, self.P11 = P00_INIT, 0.0, P11_INIT

    def step(self, z):
        self.p += self.v * DT
        self.P00 += DT * (2 * self.P01 + DT * self.P11) + Q00
        self.P01 += DT * self.P11 + Q01
        self.P11 += Q11
        S = self.P00 + R
        K0, K1 = self.P00 / S, self.P01 / S
        y = z - self.p
        self.p += K0 * y
        self.v += K1 * y
        self.P11 -= K1 * self.P01
        self.P01 -= K0 * self.P01
        self.P00 -= K0 * self.P00
        return K0, K1


class KalmanInt:
    """Line-by-line copy of kalman_init() / kalman_step() in src/kalman.c."""

    def __init__(self, z):
        self.p = chk(z * F)
        self.v = 0
        self.P00, self.P01, self.P11 = P00_INIT_Q8, 0, P11_INIT_Q8

    def step(self, z):
        # predict, DT = 1/4
        self.p = chk(self.p + asr(self.v, 2))
        self.P00 = chk(self.P00 + asr(self.P01, 1) + asr(self.P11, 4) + Q00_Q8)
        self.P01 = chk(self.P01 + asr(self.P11, 2) + Q01_Q8)
        self.P11 = chk(self.P11 + Q11_Q8)
        # update
        S = chk(self.P00 + R_Q8)
        K0 = cdiv(chk(self.P00 * KF), S)
        K1 = cdiv(chk(self.P01 * KF), S)
        y = chk(z * F - self.p)
        y = max(-Y_MAX, min(Y_MAX, y))
        self.p = chk(self.p + asr(chk(K0 * y), 12))
        self.v = chk(self.v + asr(chk(K1 * y), 12))
        self.P11 = chk(self.P11 - asr(chk(K1 * self.P01), 12))
        self.P01 = chk(self.P01 - asr(chk(K0 * self.P01), 12))
        self.P00 = chk(self.P00 - asr(chk(K0 * self.P00), 12))
        return K0, K1


def height(t):
    """Rest, 1 m lift in 1 s at t = 20 s, hold, back down at t = 40 s."""
    if t < 20:
        return 0.0
    if t < 21:
        return (1 - math.cos(math.pi * (t - 20))) / 2
    if t < 40:
        return 1.0
    if t < 41:
        return (1 + math.cos(math.pi * (t - 40))) / 2
    return 0.0


def scenario(seed=1):
    """(t, true height, true p, sensor reading) every DT."""
    rng = random.Random(seed)
    p0 = 101390.0
    iir = p0
    out = []
    t_sensor = 0.0
    for i in range(int(60 / DT)):
        t = i * DT
        while t_sensor <= t:                 # sensor runs on its own clock
            raw = p0 - height(t_sensor) / M_PER_PA + rng.gauss(0, SIGMA_RAW)
            iir += (raw - iir) / IIR_COEF
            t_sensor += 1 / ODR_HZ
        h = height(t)
        out.append((t, h, p0 - h / M_PER_PA, round(iir)))
    return out


def running_average(samples):
    """The filter this replaces: p_sum += p - p_sum/8 (main.c, AVG_SHIFT 3)."""
    s = samples[0][3] * 8
    res = []
    for _, _, _, z in samples:
        s += z - (s >> 3)
        res.append(s / 8)
    return res


def report():
    data = scenario()
    p_ref = data[0][2]
    kf, ki = KalmanFloat(data[0][3]), KalmanInt(data[0][3])
    rows = []
    for t, h, _, z in data:
        kf.step(z)
        K0, K1 = ki.step(z)
        rows.append((t, h, kf.p, kf.v, ki.p / F, ki.v / F, K0, K1))
    avg = running_average(data)

    def alt(p):
        return (p_ref - p) * M_PER_PA

    rest = [i for i, r in enumerate(rows) if 10 <= r[0] < 20]
    noise_raw = statistics_std([alt(data[i][3]) for i in rest])
    noise_avg = statistics_std([alt(avg[i]) for i in rest])
    noise_kf = statistics_std([alt(rows[i][4]) for i in rest])
    vnoise = statistics_std([-rows[i][5] * M_PER_PA for i in rest])

    def lag(series):
        """delay of the 0.5 m crossing behind the true one (t = 20.5 s)"""
        for (t, *_), val in zip(rows, series):
            if t >= 20 and val >= 0.5:
                return t - 20.5
        return float("nan")

    lag_sensor = lag([alt(d[3]) for d in data])
    lag_avg = lag([alt(a) for a in avg])
    lag_kf = lag([alt(r[4]) for r in rows])
    vmax = max(-r[5] * M_PER_PA for r in rows)
    vtrue = math.pi / 2           # peak speed of the 1 m cosine lift, m/s
    diff_p = max(abs(r[2] - r[4]) for r in rows[20:]) * M_PER_PA
    diff_v = max(abs(r[3] - r[5]) for r in rows[20:]) * M_PER_PA

    print(f"tuning: sigma_z = {SIGMA_Z} Pa, accel = {A_MPS2} m/s^2, DT = {DT} s,"
          f" sensor IIR x{IIR_COEF:g}")
    print(f"steady-state gains: K0 = {rows[-1][6] / KF:.3f}, "
          f"K1 = {rows[-1][7] / KF:.3f} 1/s")
    print()
    print("                         raw     running avg   Kalman")
    print(f"altitude noise at rest  {noise_raw * 100:5.1f} cm  "
          f"{noise_avg * 100:5.1f} cm     {noise_kf * 100:5.1f} cm")
    print(f"lag behind a 1 m lift   {lag_sensor:5.2f} s   {lag_avg:5.2f} s      "
          f"{lag_kf:5.2f} s")
    print(f"vertical speed: noise at rest {vnoise * 100:.1f} cm/s, "
          f"peak {vmax:.2f} m/s during the lift (true peak {vtrue:.2f})")
    print(f"fixed point vs float: altitude {diff_p * 100:.2f} cm, "
          f"speed {diff_v * 100:.2f} cm/s (worst case)")


def statistics_std(xs):
    m = sum(xs) / len(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / len(xs))


def consts():
    print(f"#define KF_R\t\t{R_Q8}L\t/* {R:.3f} Pa^2 */")
    print(f"#define KF_Q00\t\t{Q00_Q8}L\t/* {Q00:.4f} */")
    print(f"#define KF_Q01\t\t{Q01_Q8}L\t/* {Q01:.4f} */")
    print(f"#define KF_Q11\t\t{Q11_Q8}L\t/* {Q11:.4f} */")
    print(f"#define KF_P00_INIT\t{P00_INIT_Q8}L")
    print(f"#define KF_P11_INIT\t{P11_INIT_Q8}L")


if __name__ == "__main__":
    derived()
    if "--consts" in sys.argv:
        consts()
    else:
        report()
