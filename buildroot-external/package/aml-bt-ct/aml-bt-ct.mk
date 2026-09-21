################################################################################
#
# aml-bt-ct - Amlogic W1 Bluetooth HCI attach tool + firmware
#
################################################################################

AML_BT_CT_VERSION = 1.0
AML_BT_CT_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-bt-ct/src
AML_BT_CT_SITE_METHOD = local
AML_BT_CT_LICENSE = PROPRIETARY

define AML_BT_CT_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) all
endef

define AML_BT_CT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/aml_bt_hciattach $(TARGET_DIR)/usr/bin/aml_bt_hciattach
	# Symlink for compatibility with 6.6 platform scripts (w1_bt_start.sh uses aml_hciattach)
	ln -sf aml_bt_hciattach $(TARGET_DIR)/usr/bin/aml_hciattach
	$(INSTALL) -D -m 0644 $(@D)/fw_out.bin $(TARGET_DIR)/etc/fw_out.bin
	$(INSTALL) -D -m 0644 $(@D)/fw_out.info $(TARGET_DIR)/etc/fw_out.info
endef

$(eval $(generic-package))
