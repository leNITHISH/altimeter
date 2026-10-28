#!/usr/bin/env bash
# Regenerate the report screenshots in report/assets/ (charts: charts.py).
#
# Needs go, freeze, glow, rsvg-convert, python3 and the Maple Mono NF font.
# Rebuilds the firmware with `make -B` for the build screenshot; never
# flashes (the avrdude shot uses the saved log in gen/logs/).
set -euo pipefail
cd "$(dirname "$0")/.."                       # repo root
OUT=report/assets
GEN=report/gen
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$OUT"/{code,terminal,tables,cards,diagrams}

(cd "$GEN" && go build -o "$TMP/pinkcode" .)
PK=$TMP/pinkcode

FREEZE=(--background '#FAF6F1' --font.family 'Maple Mono NF'
	--window --border.radius 12 --border.width 1 --border.color '#F2D3E1'
	--padding 24,32 --margin 36 --shadow.blur 18 --shadow.y 10)

# shot OUT TITLE [freeze args...] < ansi: freeze, gloss, 3x PNG
shot() {
	local out=$1 title=$2 base
	shift 2
	base=$TMP/$(basename "$out" .png)
	cat > "$base.ansi"
	# freeze waits on stdin unless it's closed
	freeze "$base.ansi" -o "$base.svg" "${FREEZE[@]}" "$@" </dev/null >/dev/null
	python3 "$GEN/gloss.py" "$base.svg" "$base.g.svg" "$title"
	# opaque white, so the soft shadow looks the same on any page or viewer
	rsvg-convert -b white -z 3 "$base.g.svg" -o "$OUT/$out"
	echo "  $OUT/$out"
}

code() {	# code OUT FILE FIRST,LAST
	"$PK" code -lines "$3" "$2" | shot "code/$1" "$2"
}

echo code
code 01-i2c-start-stop-write.png src/i2c.c 30,71
code 02-i2c-read-regs.png        src/i2c.c 117,134
code 03-bme280-init.png          src/bme280.c 44,67
code 04-bme280-compensation.png  src/bme280.c 69,105
code 05-bme280-read.png          src/bme280.c 107,122
code 06-altitude-isa-table.png   src/altitude.c 1,38
code 07-altitude-hypsometric.png src/altitude.c 40,66
code 08-kalman-model.png         src/kalman.c 4,25
code 09-kalman-step.png          src/kalman.c 50,74
code 10-timer2-tick.png          src/delay.c 29,49
code 11-lcd-nibble.png           src/lcd.c 12,38
code 12-lcd-init.png             src/lcd.c 62,82
code 13-main-loop.png            src/main.c 202,259
code 14-main-display.png         src/main.c 82,117
code 15-button-events.png        src/main.c 63,80
code 16-qnh-editor.png           src/main.c 150,177
code 17-makefile.png             Makefile 1,42
code 18-kalman-sim-bitexact.png  tools/kalman_sim.py 122,147

echo terminal
term() {	# term OUT "COMMAND" FILE [freeze args...]
	local out=$1 cmd=$2 file=$3
	shift 3
	"$PK" term -cmd "$cmd" "$file" | shot "terminal/$out" "bash" "$@"
}
run() {		# run OUT "COMMAND" [freeze args...]: run it, then shoot its output
	local out=$1 cmd=$2
	shift 2
	bash -c "$cmd" > "$TMP/run.txt" 2>&1
	term "$out" "$cmd" "$TMP/run.txt" "$@"
}
run  01-make-build.png    "make -B" --wrap 100
run  02-make-size.png     "make size"
term 03-avrdude-flash.png "make flash" "$GEN/logs/avrdude-2026-10-26.txt"
run  04-kalman-sim.png    "python3 tools/kalman_sim.py"
run  05-kalman-consts.png "python3 tools/kalman_sim.py --consts"
run  06-check-hyps.png    "python3 tools/check_hyps.py"
run  07-alt-table.png     "python3 tools/gen_alt_table.py --check"
run  08-git-log.png       "git log --oneline --reverse"

echo tables
table() {	# table OUT TITLE [width]
	"$PK" table -w "${3:-0}" "$GEN/tables/${1%.png}.md" |
		shot "tables/$1" "$2" --line-height 1.0
}
table hardware.png       "hardware"
table pins.png           "pin map"
table lcd-wiring.png     "LCD wiring"
table sensor-wiring.png  "sensor wiring"
table sensor-config.png  "BMP280 registers"
table results.png        "tested on the board"
table not-tested.png     "not tested"
table history.png        "development history"
table flash-budget.png   "flash use by module"
table flash-cuts.png     "fitting into 8 KB"
table ram-regions.png    "internal RAM"
table kalman-tuning.png  "Kalman tuning (simulation)"
table gps-vs-baro.png    "barometric vs GPS height"

echo cards
# glow renders the markdown; it needs stdin closed and colour forced when piped
CLICOLOR_FORCE=1 COLORTERM=truecolor glow -s "$GEN/pink-glow.json" -w 88 \
	"$GEN/cards/limitations.md" </dev/null | shot cards/limitations.png "accuracy and limitations"

echo diagrams
"$PK" blocks | shot diagrams/block-diagram.png "block diagram" --line-height 1.0
"$PK" lcd    | shot diagrams/lcd-views.png "LCD layouts (illustration)" --line-height 1.0
