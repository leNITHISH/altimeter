#!/usr/bin/env python3
"""Post-process a freeze SVG: glossy gradient text, bold keywords, pink
window buttons, an optional title in the window bar, and no ligatures.

    python3 gloss.py in.svg out.svg [title]
"""
import re
import sys

BOLD = {"#d61f69", "#ffffff"}           # keywords and table headers (main.go)
DOTS = ["#f472a6", "#f7a8c8", "#f3cfdf"]  # window buttons, left to right


# Maple Mono turns != -> >= 0x 16x2 into symbols, and rsvg ignores
# font-feature-settings; text in separate tspans is shaped separately, so
# give each of these characters its own tspan
LIG = set("!=<>-+*/&|:.~^%#?;_")


def unligate(m):
    attrs, text = m.group(1), m.group(2)
    toks = re.findall(r"&[a-zA-Z0-9#]+;|.", text, re.S)
    runs, cur, prev = [], "", ""
    for t in toks:
        lone = t in LIG or t.startswith("&") or (t in "xX" and prev.isdigit())
        if lone:
            if cur:
                runs.append(cur)
            runs.append(t)
            cur = ""
        else:
            cur += t
        prev = t
    if cur:
        runs.append(cur)
    if len(runs) < 2:
        return m.group(0)
    return "".join(f"<tspan{attrs}>{r}</tspan>" for r in runs)


def mix(c, other, t):
    a = [int(c[i:i + 2], 16) for i in (1, 3, 5)]
    b = [int(other[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(x + (y - x) * t):02x}" for x, y in zip(a, b))


def main():
    src, dst = sys.argv[1], sys.argv[2]
    title = sys.argv[3] if len(sys.argv) > 3 else ""
    svg = open(src).read()
    colors = sorted(set(re.findall(r'<tspan[^>]*fill="(#[0-9a-f]{6})"', svg)))

    defs = ["<defs>"]
    for c in colors:
        # glass look: a light upper half with a hard edge at the middle, then the
        # true colour, a little deeper at the base
        defs.append(
            f'<linearGradient id="g{c[1:]}" x1="0" y1="0" x2="0" y2="1">'
            f'<stop offset="0" stop-color="{mix(c, "#ffffff", 0.5)}"/>'
            f'<stop offset="0.47" stop-color="{mix(c, "#ffffff", 0.22)}"/>'
            f'<stop offset="0.53" stop-color="{c}"/>'
            f'<stop offset="1" stop-color="{mix(c, "#000000", 0.12)}"/>'
            "</linearGradient>")
    # soft pink halo under the glyphs
    defs.append(
        '<filter id="glow" x="-5%" y="-5%" width="110%" height="110%">'
        '<feGaussianBlur in="SourceAlpha" stdDeviation="1.8" result="b"/>'
        '<feFlood flood-color="#f472a6" flood-opacity="0.3"/>'
        '<feComposite in2="b" operator="in" result="halo"/>'
        '<feMerge><feMergeNode in="halo"/><feMergeNode in="SourceGraphic"/></feMerge>'
        "</filter>")
    defs.append("</defs>")

    def tspan(m):
        attrs, c = m.group(1), m.group(2)
        bold = ' font-weight="bold"' if c in BOLD else ""
        return f'<tspan{attrs}fill="url(#g{c[1:]})"{bold}'

    svg = re.sub(r'<tspan([^>]*?)fill="(#[0-9a-f]{6})"', tspan, svg)
    svg = re.sub(r"<tspan([^>]*)>([^<]+)</tspan>", unligate, svg)
    svg = svg.replace('clip-path="url(#terminalMask)"', 'filter="url(#glow)"')

    # recolour the three window buttons
    circles = iter(DOTS)
    svg = re.sub(r'(<circle[^>]*?fill=")#[0-9A-Fa-f]{6}',
                 lambda m: m.group(1) + next(circles, "#f3cfdf"), svg)

    svg = re.sub(r"(<svg[^>]*>)", lambda m: m.group(1) + "".join(defs), svg, count=1)

    # freeze's grey shadow, made pink and lighter
    svg = re.sub(r'<filter id="shadow".*?</filter>',
                 '<filter id="shadow" x="-20%" y="-20%" width="140%" height="150%">'
                 '<feGaussianBlur in="SourceAlpha" stdDeviation="14"/>'
                 '<feOffset dy="10" result="b"/>'
                 '<feFlood flood-color="#d9789f" flood-opacity="0.28"/>'
                 '<feComposite in2="b" operator="in"/>'
                 '<feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge>'
                 "</filter>", svg, flags=re.S)

    if title:
        rect = re.search(r'<rect width="([\d.]+)"[^>]*x="([\d.]+)px" y="([\d.]+)px"', svg)
        w, x, y = (float(v) for v in rect.groups())
        label = (f'<text x="{x + w / 2:.2f}" y="{y + 16.5:.2f}" text-anchor="middle" '
                 f'font-family="Maple Mono NF" font-size="13px" '
                 f'fill="#a57d90">{title}</text>')
        i = svg.rfind("</svg>")
        svg = svg[:i] + label + svg[i:]
    open(dst, "w").write(svg)


if __name__ == "__main__":
    main()
