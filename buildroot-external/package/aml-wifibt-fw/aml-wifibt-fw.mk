################################################################################
#
# aml-wifibt-fw - Amlogic W1 WiFi/BT firmware and RF config files
#
################################################################################

AML_WIFIBT_FW_VERSION = 1.0
AML_WIFIBT_FW_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-wifibt-fw/files
AML_WIFIBT_FW_SITE_METHOD = local
AML_WIFIBT_FW_LICENSE = PROPRIETARY

define AML_WIFIBT_FW_INSTALL_TARGET_CMDS
	# WiFi RF calibration files -> /lib/firmware/w1/
	# Driver reads via: #define WIFI_CONF_PATH "/lib/firmware/w1" (wifi_hal_cmd.c)
	mkdir -p $(TARGET_DIR)/lib/firmware/w1
	$(INSTALL) -D -m 0644 $(@D)/wifi/w1/aml_wifi_rf*.txt $(TARGET_DIR)/lib/firmware/w1/

	# BT firmware and config -> /lib/firmware/w1/
	# w1_bt_start.sh symlinks from /lib/firmware/w1/ to /etc/bluetooth/aml/
	$(INSTALL) -D -m 0644 $(@D)/bluetooth/aml/w1_bt_fw_uart.bin $(TARGET_DIR)/lib/firmware/w1/
	$(INSTALL) -D -m 0644 $(@D)/bluetooth/aml/aml_bt_rf.txt $(TARGET_DIR)/lib/firmware/w1/
	$(INSTALL) -D -m 0644 $(@D)/bluetooth/aml/a2dp_mode_cfg.txt $(TARGET_DIR)/lib/firmware/w1/
endef

$(eval $(generic-package))
