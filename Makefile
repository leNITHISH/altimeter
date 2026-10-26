CC      = sdcc
CFLAGS  = -mmcs51 --model-small --code-size 8192 --iram-size 256 --std-sdcc11 $(EXTRA)
BUILD   = build
SRCS    = main.c delay.c lcd.c i2c.c bme280.c altitude.c kalman.c

# serial debug prints cost ~400 B of flash and need a USB-RS232 adapter:
# make clean flash EXTRA=-DUART_DEBUG
ifneq (,$(findstring UART_DEBUG,$(EXTRA)))
SRCS   += uart.c
endif
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
