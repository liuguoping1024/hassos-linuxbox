# Amlogic utility tools - simplified Makefile for HubV3L
# Only builds the tools needed for WiFi/BT module loading and system management

CFLAGS += -Wall
LDFLAGS +=

# Core tools for HubV3L
UTILS = wifi_power multi_wifi_load_driver usb_monitor

ifeq ($(USE_SIMULATE_KEY),y)
UTILS += simulate_key
endif

.PHONY: all install clean

wifi_power: wifi_power.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

multi_wifi_load_driver: multi_wifi_load_driver.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

usb_monitor: usb_monitor.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

simulate_key: simulate_key.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

ioctl: ioctl.c
	$(CC) -g3 -Wall -o $@ $^ -static

all: $(UTILS)

clean:
	rm -f *.o $(UTILS)

install:
	install -m 755 $(UTILS) $(TARGET_DIR)/usr/bin/
