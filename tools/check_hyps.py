#!/usr/bin/env python3
"""Check the 8051 integer version of the hypsometric altitude formula.

    h = 29.27 m/K * T * ln(p0 / p)                     (exact, isothermal air)
    ln(p0 / p) ~ 2 (p0 - p) / (p0 + p)                 (used on the 8051)

altitude_hyps_dm() in src/altitude.c computes, in 32-bit integers,
    k = 5854 * (t + 27315) / 8000      t in 0.01 degC, k = H/8 in 0.1 m
    h = k * (p0 - p) / ((p0 + p) >> 3) in 0.1 m, rounded to nearest
This script runs the same integer steps (with C's truncating division)
and compares them with the exact float formula.
"""
import math

DP_MAX = 80000


def cdiv(a, b):
    """C99 integer division: truncates toward zero."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def hyps_c(p, p0, t):
    k = cdiv(5854 * (t + 27315), 8000)
    dp = max(-DP_MAX, min(DP_MAX, p0 - p))
    prod = k * dp
    assert -2**31 <= prod < 2**31, "int32 overflow"
    d = (p0 + p) >> 3
    prod += d // 2 if prod >= 0 else -(d // 2)     # round, don't truncate
    return cdiv(prod, d)


def hyps_exact(p, p0, t):
    return 29.27 * (t / 100 + 273.15) * math.log(p0 / p)


def isa(p):
    return 44330.0 * (1.0 - (p / 101325.0) ** 0.1903)


if __name__ == "__main__":
    worst = 0.0
    for t in range(0, 4501, 250):                  # 0 .. 45 degC
        for dp in range(-2000, 20001, 37):         # about -170 m .. +1.8 km
            p0 = 101325
            h_c = hyps_c(p0 - dp, p0, t) / 10
            h_x = hyps_exact(p0 - dp, p0, t)
            err = abs(h_c - h_x)
            if abs(h_x) < 300:
                worst = max(worst, err)
    print(f"max |error| within +/-300 m: {worst:.3f} m")

    print("\n1 m lift (12 Pa) and 100 m (1190 Pa) at different temperatures:")
    print("  T degC   ISA      compensated   exact")
    for t in (0, 1500, 2500, 3280, 4000):
        for dp in (12, 1190):
            p0 = 101390
            print(f"  {t/100:5.1f} {isa(p0 - dp) - isa(p0):8.2f} m"
                  f" {hyps_c(p0 - dp, p0, t) / 10:8.1f} m"
                  f"   {hyps_exact(p0 - dp, p0, t):8.2f} m")

    t = 3280
    r = hyps_exact(101390 - 1190, 101390, t) / (isa(101390 - 1190) - isa(101390))
    print(f"\nat 32.8 degC the ISA formula reads {100 * (1 - 1 / r):.1f}% low")
