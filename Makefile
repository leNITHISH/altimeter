CC      = sdcc
CFLAGS  = -mmcs51 --model-small --code-size 8192 --iram-size 256 --std-sdcc11
BUILD   = build
SRCS    = main.c delay.c uart.c lcd.c
OBJS    = $(addprefix $(BUILD)/,$(SRCS:.c=.rel))
HDRS    = $(wildcard src/*.h)

HEX     = $(BUILD)/main.hex

all: $(HEX)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.rel: src/%.c $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# main.rel must come first so the startup code and main() link correctly
$(BUILD)/main.ihx: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(HEX): $(BUILD)/main.ihx
	packihx $< > $@

flash: $(HEX)
	avrdude -p 89S52 -c usbasp -e -U flash:w:$(HEX):i

size: $(BUILD)/main.ihx
	@cat $(BUILD)/main.mem
	@printf 'Flash used: %d of 8192 bytes\n' \
		$$(grep -E 'ROM/EPROM/FLASH' $(BUILD)/main.mem | awk '{print $$4}')

clean:
	rm -rf $(BUILD)

.PHONY: all flash size clean
