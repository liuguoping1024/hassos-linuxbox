CFLAGS += -Wall -g
SRC = aml_log.c
TARGET = libaml_log.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC \
	-o $@ $^ $(LDLIBS)

.PHONY: clean

clean:
	rm $(TARGET)

install:
	install -m 0644 libaml_log.so $(TARGET_DIR)/usr/lib/
	install -m 0644 aml_log.h $(STAGING_DIR)/usr/include/
	install -m 0644 libaml_log.so $(STAGING_DIR)/usr/lib/

uninstall:
	rm -f $(TARGET_DIR)/usr/lib/libaml_log.so
