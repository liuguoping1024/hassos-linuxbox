# Amlogic U-Boot environment access tool
# Builds libubootenv.a and uenv command-line tool

CFLAGS += -Wall

.PHONY: all clean install

all: ubootenv.o uenv_test.o
	$(AR) rc libubootenv.a ubootenv.o
	$(CC) ubootenv.o uenv_test.o -lz -fPIC -o uenv

ubootenv.o: ubootenv.c
	$(CC) $(CFLAGS) -fPIC -c ubootenv.c -o $@

uenv_test.o: uenv_test.c
	$(CC) $(CFLAGS) -c uenv_test.c -o $@

clean:
	rm -f *.o libubootenv.a uenv

install:
	install -D -m 0755 uenv $(TARGET_DIR)/usr/bin/uenv
	install -D -m 0644 libubootenv.a $(STAGING_DIR)/usr/lib/libubootenv.a
	install -D -m 0644 ubootenv.h $(STAGING_DIR)/usr/include/ubootenv.h
