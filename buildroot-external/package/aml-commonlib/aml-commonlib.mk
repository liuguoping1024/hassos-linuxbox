################################################################################
#
# aml-commonlib - Amlogic common library (aml_log + aml_socketipc)
#
################################################################################

AML_COMMONLIB_VERSION = 1.0
AML_COMMONLIB_SITE = $(BR2_EXTERNAL_HASSOS_PATH)/package/aml-commonlib/src
AML_COMMONLIB_SITE_METHOD = local
AML_COMMONLIB_LICENSE = PROPRIETARY
AML_COMMONLIB_INSTALL_STAGING = YES

define AML_COMMONLIB_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) all
endef

define AML_COMMONLIB_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 $(@D)/libaml_log.so $(STAGING_DIR)/usr/lib/libaml_log.so
	$(INSTALL) -D -m 0644 $(@D)/libaml_socketipc.so $(STAGING_DIR)/usr/lib/libaml_socketipc.so
	$(INSTALL) -D -m 0644 $(@D)/aml_log.h $(STAGING_DIR)/usr/include/aml_log.h
	$(INSTALL) -D -m 0644 $(@D)/aml_socketipc.h $(STAGING_DIR)/usr/include/aml_socketipc.h
	$(INSTALL) -D -m 0644 $(@D)/socketipc.h $(STAGING_DIR)/usr/include/socketipc.h
endef

define AML_COMMONLIB_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/libaml_log.so $(TARGET_DIR)/usr/lib/libaml_log.so
	$(INSTALL) -D -m 0755 $(@D)/libaml_socketipc.so $(TARGET_DIR)/usr/lib/libaml_socketipc.so
endef

$(eval $(generic-package))
