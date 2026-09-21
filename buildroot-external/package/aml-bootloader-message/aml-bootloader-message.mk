################################################################################
#
# aml-bootloader-message - Amlogic bootloader message (misc partition) tools
#
################################################################################

AML_BOOTLOADER_MESSAGE_VERSION = 1.0
AML_BOOTLOADER_MESSAGE_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-bootloader-message/src
AML_BOOTLOADER_MESSAGE_SITE_METHOD = local
AML_BOOTLOADER_MESSAGE_LICENSE = PROPRIETARY
AML_BOOTLOADER_MESSAGE_DEPENDENCIES = zlib
AML_BOOTLOADER_MESSAGE_INSTALL_STAGING = YES

define AML_BOOTLOADER_MESSAGE_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) all
endef

define AML_BOOTLOADER_MESSAGE_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 $(@D)/libbootloader_message.a $(STAGING_DIR)/usr/lib/libbootloader_message.a
	$(INSTALL) -D -m 0644 $(@D)/bootloader_message.h $(STAGING_DIR)/usr/include/bootloader_message.h
	$(INSTALL) -D -m 0644 $(@D)/bootloader_avb.h $(STAGING_DIR)/usr/include/bootloader_avb.h
endef

define AML_BOOTLOADER_MESSAGE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/urlmisc $(TARGET_DIR)/usr/bin/urlmisc
endef

$(eval $(generic-package))
