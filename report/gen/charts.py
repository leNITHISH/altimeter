#!/usr/bin/env python3
"""Charts for the report, in the same pink-on-off-white look as the
screenshots. Plain Python writing SVG (no packages, like tools/), turned
into PNG by rsvg-convert.

Everything here is host-side simulation or maths from tools/, not logged
sensor data: there was no serial adapter to log with.

    python3 report/gen/charts.py      # writes report/assets/charts/*.png
"""
import math
import os
import subprocess
import sys
import tempfile
from xml.sax.saxutils import escape

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.dont_write_bytecode = True     # leave tools/__pycache__ alone
import check_hyps  # noqa: E402
import gen_alt_table  # noqa: E402
import kalman_sim as ks  # noqa: E402

OUT = os.path.join(ROOT, "report", "assets", "charts")

PAPER = "#FAF6F1"
INK = "#5A1F45"       # titles, labels, values
MUTED = "#8C5A74"     # ticks, subtitles
GRID = "#EADDE3"
PINK = "#D61F69"      # the series the chart is about
VIOLET = "#6B5CA5"    # second series (colour-blind check passed against PINK)
GREY = "#B9A9B1"      # raw readings, context only
REF = "#5A1F45"       # true value, dashed
FONT = "Maple Mono NF"


def nice_ticks(lo, hi, n=5):
    raw = (hi - lo) / n
    mag = 10 ** math.floor(math.log10(raw))
    step = min((s * mag for s in (1, 2, 2.5, 5, 10) if s * mag >= raw))
    first = math.ceil(lo / step - 1e-9) * step
    ticks = []
    v = first
    while v <= hi + 1e-9:
        ticks.append(round(v, 10))
        v += step
    return ticks


def fmt(v):
    return f"{v:g}".replace("-", "−")


class Plot:
    """One chart: title, subtitle, a plot area with hairline grid, marks."""

    def __init__(self, title, subtitle, xlabel, ylabel, xlim, ylim,
                 w=820, h=380, left=78, top=78, xticks=None, yticks=None):
        self.w, self.h = w, h
        self.l, self.r, self.t, self.b = left, 28, top, 56
        self.xlim, self.ylim = xlim, ylim
        self.axes = []   # grid, ticks, axis labels
        self.marks = []  # data, clipped to the plot area
        self.top = []    # titles, notes, legend
        self.legend_items = []
        pw, ph = w - self.l - self.r, h - self.t - self.b
        self.pw, self.ph = pw, ph

        g = self.axes.append
        for y in yticks if yticks is not None else nice_ticks(*ylim):
            py = self.y(y)
            g(f'<line x1="{self.l}" x2="{self.l + pw}" y1="{py:.1f}" y2="{py:.1f}" '
              f'stroke="{GRID}" stroke-width="1"/>')
            g(self._text(self.l - 10, py + 4, fmt(y), 12, MUTED, anchor="end"))
        for x in xticks if xticks is not None else nice_ticks(*xlim):
            px = self.x(x)
            g(self._text(px, self.t + ph + 20, fmt(x), 12, MUTED, anchor="middle"))
        g(self._text(self.l + pw / 2, h - 10, xlabel, 12.5, INK, anchor="middle"))
        g(f'<text transform="translate(18 {self.t + ph / 2:.1f}) rotate(-90)" '
          f'text-anchor="middle" font-family="{FONT}" font-size="12.5" fill="{INK}">'
          f'{escape(ylabel)}</text>')
        self.top.append(self._text(self.l - 60, 30, title, 17, INK, bold=True))
        self.top.append(self._text(self.l - 60, 52, subtitle, 12, MUTED))

    def x(self, v):
        lo, hi = self.xlim
        return self.l + (v - lo) / (hi - lo) * self.pw

    def y(self, v):
        lo, hi = self.ylim
        return self.t + self.ph - (v - lo) / (hi - lo) * self.ph

    @staticmethod
    def _text(x, y, s, size, color, anchor="start", bold=False):
        wt = ' font-weight="bold"' if bold else ""
        return (f'<text x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}" font-family="{FONT}" '
                f'font-size="{size}" fill="{color}"{wt}>{escape(s)}</text>')

    def line(self, xs, ys, color, width=2.0, dash=None, label=None):
        pts = " ".join(f"{self.x(a):.1f},{self.y(b):.1f}" for a, b in zip(xs, ys)
                       if self.xlim[0] <= a <= self.xlim[1])
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.marks.append(f'<polyline points="{pts}" fill="none" stroke="{color}" '
                         f'stroke-width="{width}" stroke-linejoin="round" '
                         f'stroke-linecap="round"{d}/>')
        if label:
            self.legend_items.append((label, color, width, dash))

    def band(self, xs, lo, hi, color):
        """Filled min..max envelope with a 1 px edge."""
        top = [f"{self.x(a):.1f},{self.y(b):.1f}" for a, b in zip(xs, hi)]
        bot = [f"{self.x(a):.1f},{self.y(b):.1f}" for a, b in zip(reversed(xs), reversed(lo))]
        self.marks.append(f'<polygon points="{" ".join(top + bot)}" fill="{color}" '
                          f'fill-opacity="0.22" stroke="{color}" stroke-width="1" '
                          f'stroke-linejoin="round"/>')

    def hline(self, v, color=GRID, width=1.5):
        py = self.y(v)
        self.marks.insert(0, f'<line x1="{self.l}" x2="{self.l + self.pw}" y1="{py:.1f}" '
                             f'y2="{py:.1f}" stroke="{color}" stroke-width="{width}"/>')

    def vline(self, v, color=GRID, width=1):
        px = self.x(v)
        self.axes.append(f'<line x1="{px:.1f}" x2="{px:.1f}" y1="{self.t}" '
                         f'y2="{self.t + self.ph}" stroke="{color}" stroke-width="{width}"/>')

    def dot(self, x, y, color, note=None, dx=10, dy=4, anchor="start"):
        self.marks.append(f'<circle cx="{self.x(x):.1f}" cy="{self.y(y):.1f}" r="5" '
                         f'fill="{color}" stroke="{PAPER}" stroke-width="2"/>')
        if note:
            self.top.append(self._text(self.x(x) + dx, self.y(y) + dy, note, 12, INK, anchor))

    def legend(self, row=False):
        """Keys in a column at the top right of the plot area, or in one row
        above it (needs top= space) when the data fills the corners."""
        width = max(len(s) for s, *_ in self.legend_items) * 7.3 + 38
        x, y = self.l + self.pw - width, self.t + 6
        if row:
            x, y = self.l - 60, self.t - 34
        for i, (label, color, w, dash) in enumerate(self.legend_items):
            yy = y + i * 20
            if row:
                yy = y
                if i:
                    x += len(self.legend_items[i - 1][0]) * 7.3 + 58
            d = f' stroke-dasharray="{dash}"' if dash else ""
            self.top.append(f'<line x1="{x:.1f}" x2="{x + 24:.1f}" y1="{yy + 8:.1f}" '
                            f'y2="{yy + 8:.1f}" stroke="{color}" stroke-width="{max(w, 2)}" '
                            f'stroke-linecap="round"{d}/>')
            self.top.append(self._text(x + 32, yy + 12, label, 12, INK))

    def save(self, name):
        clip = (f'<clipPath id="pa"><rect x="{self.l}" y="{self.t - 2}" '
                f'width="{self.pw}" height="{self.ph + 4}"/></clipPath>')
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}">'
               f'<defs>{clip}</defs>'
               f'<rect width="{self.w}" height="{self.h}" fill="{PAPER}"/>'
               f'{"".join(self.axes)}'
               f'<g clip-path="url(#pa)">{"".join(self.marks)}</g>'
               f'{"".join(self.top)}</svg>')
        render(svg, name)


def render(svg, name):
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False) as f:
        f.write(svg)
    try:
        subprocess.run(["rsvg-convert", "-z", "3", "-b", PAPER, f.name,
                        "-o", os.path.join(OUT, name)], check=True)
    finally:
        os.unlink(f.name)
    print("  report/assets/charts/" + name)


def kalman_run():
    """The 1 m lift scenario from kalman_sim.py through the bit-exact filter."""
    ks.derived()
    data = ks.scenario()
    p_ref = data[0][2]
    kf = ks.KalmanInt(data[0][3])
    est_p, est_v = [], []
    for *_, z in data:
        kf.step(z)
        est_p.append(kf.p / ks.F)
        est_v.append(kf.v / ks.F)
    avg = ks.running_average(data)

    def alt(p):
        return (p_ref - p) * ks.M_PER_PA

    return {
        "t": [d[0] for d in data],
        "true": [d[1] for d in data],
        "raw": [alt(d[3]) for d in data],
        "avg": [alt(a) for a in avg],
        "kf": [alt(p) for p in est_p],
        "kf_v": [-v * ks.M_PER_PA for v in est_v],
    }


def true_speed(t):
    if 20 <= t < 21:
        return math.pi / 2 * math.sin(math.pi * (t - 20))
    if 40 <= t < 41:
        return -math.pi / 2 * math.sin(math.pi * (t - 40))
    return 0.0


DASH = "6 5"


def chart_lift(r):
    p = Plot("Lifting the sensor 1 m and back (simulation)",
             "tools/kalman_sim.py: BMP280 noise model, sensor IIR x8, one filter step every 0.25 s",
             "time (s)", "altitude (m)", (10, 55), (-0.3, 1.3), h=410, top=108)
    p.line(r["t"], r["raw"], GREY, 1.2, label="sensor reading")
    p.line(r["t"], r["avg"], VIOLET, label="running average (before)")
    p.line(r["t"], r["kf"], PINK, label="Kalman filter (firmware maths)")
    p.line(r["t"], r["true"], REF, 1.5, DASH, label="true height")
    p.legend(row=True)
    p.save("kalman-lift.png")


def chart_speed(r):
    fine = [i / 50 for i in range(500, 2751)]
    p = Plot("Vertical speed during the same lift (simulation)",
             "the filter smooths short moves, so the peak reads lower than the real speed",
             "time (s)", "vertical speed (m/s)", (10, 55), (-1.8, 1.8), h=340)
    p.hline(0)
    p.line(fine, [true_speed(t) for t in fine], REF, 1.5, DASH, label="true speed")
    p.line(r["t"], r["kf_v"], PINK, label="Kalman estimate (V: view)")
    p.legend()
    p.save("kalman-speed.png")


def chart_isa_error():
    p0, dp = 101390, 1190            # about a 100 m climb
    isa = check_hyps.isa(p0 - dp) - check_hyps.isa(p0)

    def err(t):
        true = check_hyps.hyps_exact(p0 - dp, p0, t * 100)
        return 100 * (isa - true) / true

    temps = [t / 10 for t in range(0, 451)]
    p = Plot("Why line 1 uses the measured temperature",
             "error of the 15 °C standard-atmosphere (ISA) formula on a 100 m climb",
             "air temperature (°C)", "ISA error (% of height)", (0, 45), (-10, 8))
    p.hline(0)
    p.line(temps, [err(t) for t in temps], PINK)
    p.dot(15, err(15), PINK, "15 °C: what ISA assumes", dx=10, dy=-10)
    v = err(32.8)
    p.dot(32.8, v, PINK, f"32.8 °C (room, measured): {fmt(round(v, 1))}%", dx=-12, dy=24,
          anchor="end")
    p.save("isa-vs-temperature.png")


def chart_hyps_int():
    p0, t = 101325, 3280
    hs, errs = [], []
    for dp in range(-2000, 20001, 3):
        exact = check_hyps.hyps_exact(p0 - dp, p0, t)
        hs.append(exact)
        errs.append(100 * (check_hyps.hyps_c(p0 - dp, p0, t) / 10 - exact))
    p = Plot("32-bit integer altitude vs the exact formula",
             "altitude_hyps_dm() at 32.8 °C: within 10 cm up to 300 m, 1.1 m low at 1 km (ln shortcut)",
             "height above the reference (m)", "firmware − exact (cm)",
             (-200, 1800), (-650, 50), yticks=[-600, -400, -200, 0])
    p.hline(0)
    p.line(hs, errs, PINK, 1)
    p.save("hypsometric-integer-error.png")


def chart_table():
    # every pascal, summarised as the min..max error per 32 Pa (~3 m) bin:
    # the 0.1 m output step makes a raw plot a solid smear
    tab = gen_alt_table.table()
    hs, lo, hi = [], [], []
    for start in range(gen_alt_table.PMIN, gen_alt_table.PMAX, 32):
        errs = [100 * (gen_alt_table.interp(tab, pa) / 10 - gen_alt_table.h(pa))
                for pa in range(start, start + 32)]
        hs.append(gen_alt_table.h(start + 16))
        lo.append(min(errs))
        hi.append(max(errs))
    worst = max(max(hi), -min(lo))
    p = Plot("21-entry lookup table for the ISA view",
             f"interpolating every 1024 Pa (grid) instead of powf(): worst {worst:.0f} cm,"
             " mostly the 0.1 m output step",
             "ISA altitude (m)", "table − exact (cm)", (-450, 1300), (-20, 20))
    for pa in range(gen_alt_table.PMIN, gen_alt_table.PMAX + 1, gen_alt_table.STEP):
        if -450 <= gen_alt_table.h(pa) <= 1300:
            p.vline(gen_alt_table.h(pa))
    p.hline(0)
    p.band(hs, lo, hi, PINK)
    p.save("isa-table-error.png")


def flash_total():
    """Flash bytes from the last build's map, if there is one."""
    try:
        for line in open(os.path.join(ROOT, "build", "main.mem")):
            if "ROM/EPROM/FLASH" in line:
                return int(line.split()[3])
    except OSError:
        pass
    return None


def chart_flash():
    # per-module sizes from the README memory budget (sdcc 4.6.0)
    parts = [("bme280.c", 2549), ("main.c", 2093), ("altitude.c", 897),
             ("runtime + startup", 878), ("kalman.c", 874), ("i2c.c", 500),
             ("lcd.c", 249), ("delay.c", 78)]
    total = sum(v for _, v in parts)
    built = flash_total()
    if built is not None and built != total:
        print(f"  warning: modules add up to {total} B but build/main.mem says {built} B")
    row, w = 34, 820
    h = 96 + row * len(parts) + 40
    left, right = 190, 90
    scale = (w - left - right) / 2600
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}">',
         f'<rect width="{w}" height="{h}" fill="{PAPER}"/>',
         Plot._text(18, 30, "Where the 8 KB of flash went", 17, INK, bold=True),
         Plot._text(18, 52, f"{total} of 8192 bytes used, {8192 - total} free "
                    "(sdcc 4.6.0, make size)", 12, MUTED)]
    for i, (name, v) in enumerate(parts):
        y = 84 + i * row
        bw = v * scale
        s.append(Plot._text(left - 12, y + 16, name, 12.5, INK, anchor="end"))
        # 4 px rounded data end, square at the baseline
        s.append(f'<rect x="{left}" y="{y + 4}" width="{bw:.1f}" height="18" rx="4" fill="{PINK}"/>')
        s.append(f'<rect x="{left}" y="{y + 4}" width="{min(bw, 8):.1f}" height="18" fill="{PINK}"/>')
        s.append(Plot._text(left + bw + 8, y + 17, f"{v} B", 12, INK))
    s.append(f'<line x1="{left}" x2="{left}" y1="80" y2="{84 + row * len(parts)}" '
             f'stroke="{GRID}" stroke-width="1.5"/>')
    s.append("</svg>")
    render("".join(s), "flash-by-module.png")


def main():
    os.makedirs(OUT, exist_ok=True)
    r = kalman_run()
    chart_lift(r)
    chart_speed(r)
    chart_isa_error()
    chart_hyps_int()
    chart_table()
    chart_flash()


if __name__ == "__main__":
    main()
