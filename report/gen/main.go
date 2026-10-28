// pinkcode turns source files, command output and a few small diagrams into
// truecolor ANSI text in the report's pink palette, for freeze to render.
//
//	pinkcode code  -lang c -lines 72,105 ../../src/bme280.c
//	pinkcode term  -cmd "make size" out.txt
//	pinkcode table -w 100 tables/pins.md
//	pinkcode lcd
//	pinkcode blocks
package main

import (
	"flag"
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"

	"github.com/alecthomas/chroma/v2"
	"github.com/alecthomas/chroma/v2/lexers"
	"github.com/charmbracelet/lipgloss"
	"github.com/charmbracelet/lipgloss/table"
	"github.com/muesli/termenv"
)

// palette, all at least 3:1 on the #FAF6F1 page (body text 9:1)
var (
	ink    = lipgloss.Color("#7A1A50")
	hot    = lipgloss.Color("#D61F69")
	magent = lipgloss.Color("#B5147C")
	rose   = lipgloss.Color("#C2336F")
	coral  = lipgloss.Color("#DB3D73")
	plum   = lipgloss.Color("#9B2A85")
	berry  = lipgloss.Color("#A8306A")
	mauve  = lipgloss.Color("#8C5A74")
	dusty  = lipgloss.Color("#A57D90")
	blush  = lipgloss.Color("#D8B4C6")
	petal  = lipgloss.Color("#FBE3EE")
	paper  = lipgloss.Color("#FAF6F1")
)

var (
	sInk   = lipgloss.NewStyle().Foreground(ink)
	sKw    = lipgloss.NewStyle().Foreground(hot).Bold(true)
	sType  = lipgloss.NewStyle().Foreground(magent)
	sFunc  = lipgloss.NewStyle().Foreground(rose)
	sNum   = lipgloss.NewStyle().Foreground(coral)
	sStr   = lipgloss.NewStyle().Foreground(berry)
	sPre   = lipgloss.NewStyle().Foreground(plum)
	sConst = lipgloss.NewStyle().Foreground(berry)
	sOp    = lipgloss.NewStyle().Foreground(mauve)
	sCmt   = lipgloss.NewStyle().Foreground(dusty).Italic(true)
	sLn    = lipgloss.NewStyle().Foreground(blush)
)

const tabWidth = 4

func main() {
	lipgloss.SetColorProfile(termenv.TrueColor)
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: pinkcode code|term|table|lcd|blocks ...")
		os.Exit(2)
	}
	args := os.Args[2:]
	switch os.Args[1] {
	case "code":
		fs := flag.NewFlagSet("code", flag.ExitOnError)
		lang := fs.String("lang", "", "chroma lexer name (default: from file name)")
		lines := fs.String("lines", "", "first,last line to show (1-based)")
		fs.Parse(args)
		code(fs.Arg(0), *lang, *lines)
	case "term":
		fs := flag.NewFlagSet("term", flag.ExitOnError)
		cmd := fs.String("cmd", "", "command line shown after the prompt")
		fs.Parse(args)
		term(*cmd, fs.Arg(0))
	case "table":
		fs := flag.NewFlagSet("table", flag.ExitOnError)
		width := fs.Int("w", 0, "wrap the table to this many columns")
		fs.Parse(args)
		mdTable(fs.Arg(0), *width)
	case "lcd":
		lcd()
	case "blocks":
		blocks()
	default:
		fmt.Fprintln(os.Stderr, "unknown mode", os.Args[1])
		os.Exit(2)
	}
}

func must(err error) {
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

var allCaps = regexp.MustCompile(`^[A-Z][A-Z0-9_]+$`)

// style picks a colour for one token; next is the following non-space token,
// so a name followed by "(" can be shown as a function call
func style(t chroma.Token, next string) lipgloss.Style {
	tt := t.Type
	switch {
	case tt.InCategory(chroma.Comment):
		if tt == chroma.CommentPreproc || tt == chroma.CommentPreprocFile {
			return sPre
		}
		return sCmt
	case tt == chroma.KeywordType:
		return sType
	case tt.InCategory(chroma.Keyword):
		return sKw
	case tt.InCategory(chroma.LiteralString):
		return sStr
	case tt.InCategory(chroma.LiteralNumber):
		return sNum
	case tt.InCategory(chroma.Operator), tt == chroma.Punctuation:
		return sOp
	case tt.InCategory(chroma.Name):
		v := strings.TrimSpace(t.Value)
		switch {
		case strings.HasPrefix(v, "__"): // sdcc storage classes
			return sKw
		case strings.HasSuffix(v, "_t"):
			return sType
		case tt == chroma.NameFunction || strings.HasPrefix(next, "("):
			return sFunc
		case allCaps.MatchString(v):
			return sConst
		case tt == chroma.NameBuiltin:
			return sType
		}
	}
	return sInk
}

// expand replaces tabs, tracking the column across tokens
func expand(s string, col *int) string {
	var b strings.Builder
	for _, r := range s {
		if r == '\t' {
			n := tabWidth - *col%tabWidth
			b.WriteString(strings.Repeat(" ", n))
			*col += n
			continue
		}
		b.WriteRune(r)
		*col++
	}
	return b.String()
}

func code(path, lang, span string) {
	src, err := os.ReadFile(path)
	must(err)
	var lx chroma.Lexer
	if lang != "" {
		lx = lexers.Get(lang)
	} else {
		lx = lexers.Match(path)
	}
	if lx == nil {
		lx = lexers.Fallback
	}
	lx = chroma.Coalesce(lx)
	it, err := lx.Tokenise(nil, string(src))
	must(err)
	lines := chroma.SplitTokensIntoLines(it.Tokens())

	first, last := 1, len(lines)
	if span != "" {
		parts := strings.Split(span, ",")
		first, _ = strconv.Atoi(parts[0])
		last, _ = strconv.Atoi(parts[1])
	}
	width := len(strconv.Itoa(last))
	for n := first; n <= last && n <= len(lines); n++ {
		toks := lines[n-1]
		var b strings.Builder
		b.WriteString(sLn.Render(fmt.Sprintf("%*d", width, n)) + "   ")
		col := 0
		for i, t := range toks {
			next := ""
			for _, u := range toks[i+1:] {
				if v := strings.TrimSpace(u.Value); v != "" {
					next = v
					break
				}
			}
			text := strings.TrimRight(expand(t.Value, &col), "\n")
			if strings.TrimSpace(text) == "" {
				b.WriteString(text)
				continue
			}
			if t.Type == chroma.CommentPreproc {
				b.WriteString(preproc(text))
				continue
			}
			b.WriteString(style(t, next).Render(text))
		}
		fmt.Println(strings.TrimRight(b.String(), " "))
	}
}

var ppRe = regexp.MustCompile(`#\s*\w+|0x[0-9A-Fa-f]+[UL]*|\b\d+[UL]*\b|[A-Za-z_]\w*|[^\w\s]+|\s+`)

// preproc colours the parts of a "#define NAME value" line that chroma
// returns as one token
func preproc(s string) string {
	var b strings.Builder
	for _, w := range ppRe.FindAllString(s, -1) {
		switch {
		case strings.TrimSpace(w) == "":
			b.WriteString(w)
		case w[0] == '#':
			b.WriteString(sPre.Render(w))
		case w[0] >= '0' && w[0] <= '9':
			b.WriteString(sNum.Render(w))
		case allCaps.MatchString(w):
			b.WriteString(sConst.Render(w))
		case isWord(w[0]):
			b.WriteString(sInk.Render(w))
		default:
			b.WriteString(sOp.Render(w))
		}
	}
	return b.String()
}

var numRe = regexp.MustCompile(`(0x[0-9A-Fa-f]+|[-+]?\d+(\.\d+)?%?)`)

// term shows a prompt line and then the captured output, with numbers in pink
func term(cmd, path string) {
	out, err := os.ReadFile(path)
	must(err)
	prompt := lipgloss.NewStyle().Foreground(hot).Bold(true).Render("❯")
	fmt.Println(prompt + " " + lipgloss.NewStyle().Foreground(plum).Bold(true).Render(cmd))
	for _, line := range strings.Split(strings.TrimRight(string(out), "\n"), "\n") {
		col := 0
		line = expand(line, &col)
		if ramRow.MatchString(line) {
			fmt.Println(ramMap(line))
			continue
		}
		var b strings.Builder
		last := 0
		for _, m := range numRe.FindAllStringIndex(line, -1) {
			// skip digits that are part of a word (e.g. BMP280, P3.3, x16, 89S52)
			if m[0] > 0 && isWord(line[m[0]-1]) || m[1] < len(line) && isWord(line[m[1]]) {
				continue
			}
			b.WriteString(sInk.Render(line[last:m[0]]))
			b.WriteString(sNum.Render(line[m[0]:m[1]]))
			last = m[1]
		}
		b.WriteString(sInk.Render(line[last:]))
		fmt.Println(b.String())
	}
}

var ramRow = regexp.MustCompile(`^0x[0-9a-f]{2}:\|`)

// ramMap colours one row of sdcc's internal RAM map by region
func ramMap(line string) string {
	var b strings.Builder
	b.WriteString(sNum.Render(line[:5]))
	for _, r := range line[5:] {
		c := string(r)
		switch {
		case r == '|':
			b.WriteString(sLn.Render(c))
		case r >= '0' && r <= '3':
			b.WriteString(sLn.Render(c)) // register banks
		case r == 'I':
			b.WriteString(sType.Render(c))
		case r == 'S':
			b.WriteString(sKw.Render(c))
		case r == 'Q', r == 'B':
			b.WriteString(sPre.Render(c))
		default:
			b.WriteString(sFunc.Render(c)) // a-z: module data
		}
	}
	return b.String()
}

func isWord(c byte) bool {
	return c == '_' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'
}

var tickRe = regexp.MustCompile("`([^`]*)`")

// cell colours `code` spans and drops markdown emphasis
func cell(s string, base lipgloss.Style) string {
	s = strings.ReplaceAll(strings.TrimSpace(s), "**", "")
	var b strings.Builder
	last := 0
	for _, m := range tickRe.FindAllStringSubmatchIndex(s, -1) {
		b.WriteString(base.Render(s[last:m[0]]))
		b.WriteString(base.Foreground(magent).Render(s[m[2]:m[3]]))
		last = m[1]
	}
	b.WriteString(base.Render(s[last:]))
	return b.String()
}

// mdTable renders a markdown pipe table with lipgloss/table
func mdTable(path string, width int) {
	src, err := os.ReadFile(path)
	must(err)
	var rows [][]string
	for _, line := range strings.Split(strings.TrimSpace(string(src)), "\n") {
		line = strings.Trim(strings.TrimSpace(line), "|")
		if strings.Trim(line, "-|: ") == "" {
			continue // the |---|---| line
		}
		rows = append(rows, strings.Split(line, "|"))
	}
	head := lipgloss.NewStyle().Foreground(lipgloss.Color("#FFFFFF")).Background(hot).Bold(true).Padding(0, 1)
	even := lipgloss.NewStyle().Foreground(ink).Padding(0, 1)
	odd := even.Background(petal)

	var hdr []string
	for _, h := range rows[0] {
		hdr = append(hdr, strings.TrimSpace(strings.ReplaceAll(h, "**", "")))
	}
	t := table.New().
		Border(lipgloss.RoundedBorder()).
		BorderStyle(lipgloss.NewStyle().Foreground(rose)).
		BorderRow(false).
		Headers(hdr...).
		StyleFunc(func(r, c int) lipgloss.Style {
			if r == table.HeaderRow {
				return head
			}
			if r%2 == 1 {
				return odd
			}
			return even
		})
	for i, r := range rows[1:] {
		base := even
		if i%2 == 1 {
			base = odd
		}
		var cells []string
		for j, c := range r {
			st := base
			if j == 0 {
				st = st.Foreground(berry)
			}
			// pre-render so inline code keeps its colour; padding comes from StyleFunc
			cells = append(cells, cell(c, st.UnsetPadding()))
		}
		t.Row(cells...)
	}
	if width > 0 {
		t.Width(width)
	}
	fmt.Println(t)
}

// lcd draws the 16x2 screens as boxes (layout illustration, not a photo)
func lcd() {
	screens := []struct{ caption, l1, l2 string }{
		{"view 1: temp + pressure", "Alt:     1.0 m R", "T 32.8C P1013.9 "},
		{"view 2: ISA comparison", "Alt:     1.0 m R", "ISA:     0.9 m  "},
		{"view 3: vertical speed", "Alt:     0.6 m R", "V:     0.3 m/s  "},
		{"view 4: QNH (absolute)", "Alt:    16.5 m A", "QNH 1013.2 hPa  "},
		{"QNH editor", "Set QNH (hPa)   ", "1013.2          "},
		{"editor: out of range", "Only 870-1085!  ", "0800.0          "},
	}
	glass := lipgloss.NewStyle().
		Background(petal).Foreground(ink).Bold(true).
		Padding(0, 1)
	bezel := lipgloss.NewStyle().
		Border(lipgloss.RoundedBorder()).BorderForeground(rose).
		Padding(0, 1)
	capt := lipgloss.NewStyle().Foreground(mauve).Italic(true).Width(26).Align(lipgloss.Center)

	var cells []string
	for _, s := range screens {
		box := bezel.Render(glass.Render(s.l1) + "\n" + glass.Render(s.l2))
		cells = append(cells, lipgloss.JoinVertical(lipgloss.Center, box, capt.Render(s.caption)))
	}
	gap := "   "
	for i := 0; i < len(cells); i += 3 {
		fmt.Println(lipgloss.JoinHorizontal(lipgloss.Top, cells[i], gap, cells[i+1], gap, cells[i+2]))
		if i+3 < len(cells) {
			fmt.Println()
		}
	}
}

// blocks draws the system block diagram
func blocks() {
	const h = 5 // body lines, so every box in a row has the same height
	box := func(title, body string, w int) string {
		t := lipgloss.NewStyle().Foreground(hot).Bold(true).Render(title)
		lines := strings.Split(body, "\n")
		for len(lines) < h-1 {
			lines = append(lines, "")
		}
		b := lipgloss.NewStyle().Foreground(ink).Render(strings.Join(lines, "\n"))
		return lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).BorderForeground(rose).
			Padding(0, 1).Width(w).
			Render(t + "\n" + b)
	}
	label := lipgloss.NewStyle().Foreground(mauve).Italic(true).Render
	pink := lipgloss.NewStyle().Foreground(hot).Render
	// a link between two boxes: label above, arrow, label below, centred
	link := func(above, arrow, below string) string {
		w := max(len([]rune(above)), len([]rune(below)), len([]rune(arrow))) + 2
		c := lipgloss.NewStyle().Width(w).Align(lipgloss.Center)
		col := []string{"", "", c.Render(label(above)), c.Render(pink(arrow)), c.Render(label(below)), "", ""}
		return strings.Join(col, "\n")
	}

	sensor := box("BMP280", "pressure and\ntemperature\nI2C address 0x76\n5 V breakout", 20)
	mcu := box("AT89S52", "11.0592 MHz\n8 KB flash, 256 B RAM\nKalman filter\nhypsometric altitude", 24)
	lcd := box("16x2 LCD", "HD44780\n4-bit bus\nRW tied to GND\n10k contrast pot", 20)
	row1 := lipgloss.JoinHorizontal(lipgloss.Top,
		sensor, link("SCL P1.0", "◀────────▶", "SDA P1.1"),
		mcu, link("RS P2.1  E P2.2", "──────────▶", "D4-D7 P2.4-P2.7"), lcd)
	mcuX := lipgloss.Width(sensor) + lipgloss.Width(link("SCL P1.0", "◀────────▶", "SDA P1.1"))
	mcuC := mcuX + lipgloss.Width(mcu)/2

	btn := box("Buttons", "P3.3 tap: zero\nP3.3 hold: absolute\nP3.2 tap: next view\nP3.2 hold: QNH", 22)
	led := box("LED (P2.0)", "toggles on every\nsensor reading", 20)
	isp := box("USBasp ISP", "avrdude from the PC\nP1.5-P1.7, RST", 22)
	gap := "    "
	row2 := lipgloss.JoinHorizontal(lipgloss.Top, btn, gap, led, gap, isp)
	c1 := lipgloss.Width(btn) / 2
	c2 := lipgloss.Width(btn) + len(gap) + lipgloss.Width(led)/2
	c3 := lipgloss.Width(btn) + 2*len(gap) + lipgloss.Width(led) + lipgloss.Width(isp)/2
	off := mcuC - c2 // shift row 2 so the LED sits under the MCU
	pad := strings.Repeat(" ", max(off, 0))

	bus := []rune(strings.Repeat(" ", c3+1))
	for x := c1; x <= c3; x++ {
		bus[x] = '─'
	}
	bus[c1], bus[c2], bus[c3] = '┌', '┼', '┐'
	tips := []rune(strings.Repeat(" ", c3+1))
	tips[c1], tips[c2], tips[c3] = '▲', '▼', '▲'
	stem := []rune(strings.Repeat(" ", c3+1))
	stem[c2] = '│'

	fmt.Println(row1)
	fmt.Println(pad + pink(string(stem)))
	fmt.Println(pad + pink(string(bus)))
	fmt.Println(pad + pink(string(tips)))
	fmt.Println(lipgloss.NewStyle().PaddingLeft(max(off, 0)).Render(row2))
}
