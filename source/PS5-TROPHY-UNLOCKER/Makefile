# SpiderKit PS5 Trophy Unlocker launcher
# GPL-3.0-or-later

ifndef PS5_PAYLOAD_SDK
$(error PS5_PAYLOAD_SDK is undefined)
endif

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

TARGET := PS5-TROPHY-UNLOCKER.ELF
ENGINE := engine/trophy_unlocker_engine.elf
OBJECTS := main.o elfldr.o pt.o notify.o engine_blob.o

CFLAGS := -O2 -Wall -Wextra -Werror

all: $(TARGET)

engine_blob.o: $(ENGINE)
	$(OBJCOPY) --input-target=binary --output-target=elf64-x86-64 \
		--binary-architecture=x86-64 $< $@

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^
	$(STRIP) $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
