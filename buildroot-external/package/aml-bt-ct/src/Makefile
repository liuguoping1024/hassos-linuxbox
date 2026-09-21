# Amlogic BT HCI attach tool and BT firmware
# Builds aml_bt_hciattach for W1 Bluetooth initialization

CFLAGS += -Wall -Wextra -std=gnu99

TARGET = aml_bt_hciattach

SRC = aml_bt_hciattach.c bt_hci.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

install:
	install -D -m 0755 $(TARGET) $(TARGET_DIR)/usr/bin/aml_bt_hciattach
	install -D -m 0644 fw_out.bin $(TARGET_DIR)/etc/fw_out.bin
	install -D -m 0644 fw_out.info $(TARGET_DIR)/etc/fw_out.info
