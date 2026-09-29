################################################################################
#
# aml-bt - Amlogic W1 Bluetooth SDIO driver (out-of-tree kernel module)
#
# Exactly mirrors the SDK's aml-bt.mk:
#   $(MAKE) -C $(LINUX_DIR) M=$(@D) KBUILD_EXTRA_SYMBOLS=...Module.symvers modules
#
################################################################################

# Vendor provenance
# -----------------
#   upstream: sdk_A113X_202210/hardware/aml-5.4/bluetooth/amlogic/aml_bt/
#             sdio_driver_bt
#   produces: sdio_bt.ko  (SDIO transport, not UART)
#
# NOTE: this is the SDIO BT driver, while the shared w1_bt_start.sh still
# attaches over UART (aml_hciattach on /dev/ttyAML1). That mismatch is
# why hci0 never appears on hubv3l even though the driver reports
# "Init sdio_bt OK!". w1_bt_start.sh needs splitting per kernel line the
# same way w1_start.sh already is.
AML_BT_VERSION = 1.0
AML_BT_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-bt/src/sdio_driver_bt
AML_BT_SITE_METHOD = local
AML_BT_LICENSE = PROPRIETARY

AML_BT_DEPENDENCIES = linux aml-wifi

AML_BT_MODULE_DIR = extra/amlogic/bt
AML_BT_INSTALL_DIR = $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/$(AML_BT_MODULE_DIR)

# Path to WiFi driver's Module.symvers (provides g_w1_hif_ops etc.)
AML_BT_WIFI_SYMVERS = $(BUILD_DIR)/aml-wifi-1.0/project_w1/vmac/Module.symvers

define AML_BT_BUILD_CMDS
	$(MAKE) -C $(LINUX_DIR) \
		M=$(@D) \
		ARCH=$(KERNEL_ARCH) \
		KBUILD_EXTRA_SYMBOLS=$(AML_BT_WIFI_SYMVERS) \
		CROSS_COMPILE=$(TARGET_CROSS) \
		modules
	$(TARGET_CROSS)strip --strip-unneeded $(@D)/sdio_bt.ko
endef

define AML_BT_INSTALL_TARGET_CMDS
	mkdir -p $(AML_BT_INSTALL_DIR)
	$(INSTALL) -m 0644 $(@D)/sdio_bt.ko $(AML_BT_INSTALL_DIR)/
	echo $(AML_BT_MODULE_DIR)/sdio_bt.ko: >> \
		$(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/modules.dep
	# BT firmware and config
	mkdir -p $(TARGET_DIR)/lib/firmware/w1
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-wifibt-fw/files/bluetooth/aml/w1_bt_fw_uart.bin \
		$(TARGET_DIR)/lib/firmware/w1/
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-wifibt-fw/files/bluetooth/aml/aml_bt_rf.txt \
		$(TARGET_DIR)/lib/firmware/w1/
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-wifibt-fw/files/bluetooth/aml/a2dp_mode_cfg.txt \
		$(TARGET_DIR)/lib/firmware/w1/
endef

$(eval $(generic-package))
