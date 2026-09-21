################################################################################
#
# aml-ubootenv - Amlogic U-Boot environment access tool
#
################################################################################

AML_UBOOTENV_VERSION = 1.0
AML_UBOOTENV_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-ubootenv/src
AML_UBOOTENV_SITE_METHOD = local
AML_UBOOTENV_LICENSE = PROPRIETARY
AML_UBOOTENV_DEPENDENCIES = zlib
AML_UBOOTENV_INSTALL_STAGING = YES

define AML_UBOOTENV_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -fPIC -c $(@D)/ubootenv.c -o $(@D)/ubootenv.o
	$(TARGET_CC) $(TARGET_CFLAGS) -c $(@D)/uenv_test.c -o $(@D)/uenv_test.o
	$(TARGET_AR) rc $(@D)/libubootenv.a $(@D)/ubootenv.o
	$(TARGET_CC) $(@D)/ubootenv.o $(@D)/uenv_test.o -lz -fPIC -o $(@D)/uenv
endef

define AML_UBOOTENV_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 $(@D)/libubootenv.a $(STAGING_DIR)/usr/lib/libubootenv.a
	$(INSTALL) -D -m 0644 $(@D)/ubootenv.h $(STAGING_DIR)/usr/include/ubootenv.h
endef

define AML_UBOOTENV_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/uenv $(TARGET_DIR)/usr/bin/uenv
endef

$(eval $(generic-package))
