################################################################################
#
# aml-wifi - Amlogic W1 WiFi SDIO driver (out-of-tree kernel module)
#
# Exactly mirrors the SDK's aml-wifi.mk:
#   $(MAKE) -C $(LINUX_DIR) M=$(@D)/project_w1/vmac modules CONFIG_BUILDROOT=y
#
################################################################################

AML_WIFI_VERSION = 1.0
AML_WIFI_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-wifi/src
AML_WIFI_SITE_METHOD = local
AML_WIFI_LICENSE = PROPRIETARY

AML_WIFI_DEPENDENCIES = linux

AML_WIFI_MODULE_DIR = kernel/amlogic/wifi
AML_WIFI_INSTALL_DIR = $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/$(AML_WIFI_MODULE_DIR)

define AML_WIFI_BUILD_CMDS
	# Generate version file (same as SDK)
	perl $(@D)/project_w1/vmac/create_version_file.pl || true
	# Build out-of-tree modules (same as SDK)
	$(MAKE) -C $(LINUX_DIR) \
		M=$(@D)/project_w1/vmac \
		ARCH=$(KERNEL_ARCH) \
		CROSS_COMPILE=$(TARGET_CROSS) \
		modules \
		CONFIG_BUILDROOT=y
	# Strip (same as SDK)
	$(TARGET_CROSS)strip --strip-unneeded $(@D)/project_w1/vmac/vlsicomm.ko
	$(TARGET_CROSS)strip --strip-unneeded $(@D)/project_w1/vmac/aml_sdio.ko
endef

define AML_WIFI_INSTALL_TARGET_CMDS
	mkdir -p $(AML_WIFI_INSTALL_DIR)
	$(INSTALL) -m 0644 $(@D)/project_w1/vmac/vlsicomm.ko $(AML_WIFI_INSTALL_DIR)/
	$(INSTALL) -m 0644 $(@D)/project_w1/vmac/aml_sdio.ko $(AML_WIFI_INSTALL_DIR)/
	# Append to modules.dep so modprobe can find them
	echo $(AML_WIFI_MODULE_DIR)/vlsicomm.ko: $(AML_WIFI_MODULE_DIR)/aml_sdio.ko >> \
		$(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/modules.dep
	echo $(AML_WIFI_MODULE_DIR)/aml_sdio.ko: >> \
		$(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/modules.dep
	# RF config -> /etc/wifi/w1 (for multi_wifi_load_driver conf_path=/etc/wifi/w1)
	# /lib/firmware/w1 is owned by aml-wifibt-fw (driver default path)
	mkdir -p $(TARGET_DIR)/etc/wifi/w1
	$(INSTALL) -m 0644 $(@D)/project_w1/vmac/aml_wifi_rf*.txt $(TARGET_DIR)/etc/wifi/w1/
endef

$(eval $(generic-package))
