# Amlogic common library - combined Makefile
# Builds libaml_log.so and libaml_socketipc.so

CFLAGS += -Wall -g

.PHONY: all clean install

all: libaml_log.so libaml_socketipc.so

libaml_log.so: aml_log.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -o $@ $^ $(LDLIBS)

libaml_socketipc.so: aml_socketipc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -o $@ $^ $(LDLIBS)

clean:
	rm -f libaml_log.so libaml_socketipc.so

install:
	install -m 0644 libaml_log.so $(TARGET_DIR)/usr/lib/
	install -m 0644 libaml_socketipc.so $(TARGET_DIR)/usr/lib/
	install -m 0644 aml_log.h $(STAGING_DIR)/usr/include/
	install -m 0644 aml_socketipc.h $(STAGING_DIR)/usr/include/
	install -m 0644 socketipc.h $(STAGING_DIR)/usr/include/
	install -m 0644 libaml_log.so $(STAGING_DIR)/usr/lib/
	install -m 0644 libaml_socketipc.so $(STAGING_DIR)/usr/lib/
