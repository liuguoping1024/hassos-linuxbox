# Amlogic bootloader message tools
# Builds urlmisc (read/write misc partition) and test_ab (A/B boot test)

CFLAGS += -Wall

LIB = libbootloader_message.a
OBJS = bootloader_message.o bootloader_avb.o avb_crc32.o

.PHONY: all clean install

all: urlmisc test_ab $(LIB)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	$(AR) rc $@ $^

urlmisc: urlmisc.o $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

test_ab: test.o $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o $(LIB) urlmisc test_ab

install:
	install -D -m 0755 urlmisc $(TARGET_DIR)/usr/bin/urlmisc
	install -D -m 0644 $(LIB) $(STAGING_DIR)/usr/lib/$(LIB)
	install -D -m 0644 bootloader_message.h $(STAGING_DIR)/usr/include/bootloader_message.h
	install -D -m 0644 bootloader_avb.h $(STAGING_DIR)/usr/include/bootloader_avb.h
