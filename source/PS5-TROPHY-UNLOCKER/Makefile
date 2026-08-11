# SpiderKit PS5 Trophy Unlocker launcher
# GPL-3.0-or-later

ifndef PS5_PAYLOAD_SDK
$(error PS5_PAYLOAD_SDK is undefined)
endif

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

TARGET := PS5-TROPHY-UNLOCKER.ELF
OBJECTS := main.o pt.o notify.o

CFLAGS := -O2 -Wall -Wextra -Werror

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^
	$(STRIP) $@

clean:
	rm -f $(TARGET) $(OBJECTS) *.plist

.PHONY: all clean
