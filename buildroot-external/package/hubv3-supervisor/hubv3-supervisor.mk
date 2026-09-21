################################################################################
#
# hubv3-supervisor
#
################################################################################

HUBV3_SUPERVISOR_VERSION = fd480b77fff455a54327c546950e6dda840606ac
HUBV3_SUPERVISOR_SITE = git@github.com:liuguoping1024/LinuxBox_Supervisor.git
HUBV3_SUPERVISOR_SITE_METHOD = git
HUBV3_SUPERVISOR_LICENSE = Proprietary
# Every REQUIRED dependency in the upstream CMakeLists.txt must be listed here,
# not merely selected in Config.in. Config.in only guarantees the package is
# *enabled*; DEPENDENCIES is what guarantees it is *built first*. An undeclared
# dependency works by luck and fails non-deterministically under -j: the
# mosquitto.h failure on hubv3 was exactly this, and util-linux/udev were in
# the same state (enabled everywhere, so never caught).
#
# Mapping from CMakeLists.txt to buildroot packages:
#   pkg_check_modules glib-2.0 gio-2.0 -> libglib2
#   pkg_check_modules json-c           -> json-c
#   pkg_check_modules avahi-client     -> avahi
#   pkg_check_modules libcurl          -> libcurl
#   pkg_check_modules openssl          -> openssl
#   pkg_check_modules uuid             -> util-linux (BR2_PACKAGE_UTIL_LINUX_LIBUUID)
#   pkg_check_modules libudev          -> udev (virtual; systemd provides it here)
#   gpiod.h / microhttpd.h / cjson     -> libgpiod / libmicrohttpd / cjson
#   sqlite3.h / yaml.h                 -> sqlite / libyaml
#   bluetooth/bluetooth.h              -> bluez5_utils
#   mosquitto.h                        -> mosquitto
HUBV3_SUPERVISOR_DEPENDENCIES = \
	host-pkgconf libglib2 json-c avahi libgpiod openssl \
	libmicrohttpd libcurl libyaml sqlite cjson bluez5_utils \
	mosquitto util-linux udev

HUBV3_SUPERVISOR_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release

define HUBV3_SUPERVISOR_INSTALL_EXTRAS
	# D-Bus policy
	$(INSTALL) -D -m 0644 $(@D)/config/dbus/com.thirdreality.linuxbox.Supervisor.conf \
		$(TARGET_DIR)/etc/dbus-1/system.d/com.thirdreality.linuxbox.Supervisor.conf
	# Default config (read-only, copied to /mnt/overlay on first boot by os-overlay)
	$(INSTALL) -d $(TARGET_DIR)/etc/hubv3-supervisor
	$(INSTALL) -D -m 0644 $(@D)/config/configuration.yaml \
		$(TARGET_DIR)/etc/hubv3-supervisor/configuration.yaml
	# Zigbee2MQTT conf templates (read-only)
	$(INSTALL) -d $(TARGET_DIR)/etc/hubv3-supervisor/conf
	cp -dpfr $(@D)/config/conf/* $(TARGET_DIR)/etc/hubv3-supervisor/conf/
	# Static web UI files (read-only, served directly from /usr/share)
	$(INSTALL) -d $(TARGET_DIR)/usr/share/hubv3-supervisor/static/css
	$(INSTALL) -d $(TARGET_DIR)/usr/share/hubv3-supervisor/static/js
	cp -dpfr $(@D)/config/static/* $(TARGET_DIR)/usr/share/hubv3-supervisor/static/
endef
HUBV3_SUPERVISOR_POST_INSTALL_TARGET_HOOKS += HUBV3_SUPERVISOR_INSTALL_EXTRAS

define HUBV3_SUPERVISOR_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(@D)/supervisor.service \
		$(TARGET_DIR)/etc/systemd/system/supervisor.service
endef

$(eval $(cmake-package))
