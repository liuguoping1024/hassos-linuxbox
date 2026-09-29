################################################################################
#
# aml-util - Amlogic system utility tools
#
################################################################################

AML_UTIL_VERSION = 1.0
AML_UTIL_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-util/src
AML_UTIL_SITE_METHOD = local
AML_UTIL_LICENSE = PROPRIETARY
AML_UTIL_DEPENDENCIES = linux libusb

# Build flags: tell the driver loader where to find Amlogic WiFi modules
AML_UTIL_CFLAGS = $(TARGET_CFLAGS) \
	-I$(STAGING_DIR)/usr/include \
	-DAMLOGIC_MODULES_PATH=/lib/modules/$(LINUX_VERSION_PROBED)/extra/amlogic/wifi

AML_UTIL_LDFLAGS = -lusb-1.0

define AML_UTIL_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) \
		CFLAGS="$(AML_UTIL_CFLAGS)" \
		LDFLAGS="$(AML_UTIL_LDFLAGS)" \
		USE_SIMULATE_KEY=y \
		all
endef

define AML_UTIL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/wifi_power $(TARGET_DIR)/usr/bin/wifi_power
	$(INSTALL) -D -m 0755 $(@D)/multi_wifi_load_driver $(TARGET_DIR)/usr/bin/multi_wifi_load_driver
	$(INSTALL) -D -m 0755 $(@D)/usb_monitor $(TARGET_DIR)/usr/bin/usb_monitor
	$(INSTALL) -D -m 0755 $(@D)/simulate_key $(TARGET_DIR)/usr/bin/simulate_key
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-util/S43sysname \
		$(TARGET_DIR)/etc/init.d/S43sysname
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-util/get_sysname.sh \
		$(TARGET_DIR)/etc/init.d/get_sysname.sh
endef

$(eval $(generic-package))
