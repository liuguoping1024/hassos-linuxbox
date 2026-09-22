################################################################################
#
# zigbee2mqtt
#
################################################################################

ZIGBEE2MQTT_VERSION = 3r_blz_2.11.0
ZIGBEE2MQTT_SITE = https://github.com/thirdreality/zigbee2mqtt.git
ZIGBEE2MQTT_SITE_METHOD = git
ZIGBEE2MQTT_LICENSE = GPL-3.0
ZIGBEE2MQTT_LICENSE_FILES = LICENSE
ZIGBEE2MQTT_DEPENDENCIES = nodejs host-nodejs zigbee-herdsman mosquitto

# zigbee-herdsman build directory (sibling of zigbee2mqtt in output/build/)
ZIGBEE_HERDSMAN_BUILD_DIR = $(BUILD_DIR)/zigbee-herdsman-$(ZIGBEE_HERDSMAN_VERSION)

# Node.js headers + node-gyp for cross-compiling native addons
# node-gyp expects: <nodedir>/include/node/node_version.h
# buildroot installs headers at: $(STAGING_DIR)/usr/include/node/node_version.h
# So we create a nodedir wrapper: <build>/.nodedir/include/node -> actual headers
ZIGBEE2MQTT_NODE_HEADERS_SRC = $(STAGING_DIR)/usr/include/node
ZIGBEE2MQTT_NODEDIR = $(@D)/.nodedir
ZIGBEE2MQTT_NODE_GYP = $(HOST_DIR)/bin/node \
	$(HOST_DIR)/lib/node_modules/npm/node_modules/node-gyp/bin/node-gyp.js

# Helper: cross-compile a native addon dir for aarch64
# Usage: $(call CROSS_COMPILE_ADDON,<path-to-addon-dir>)
define CROSS_COMPILE_ADDON
	mkdir -p $(ZIGBEE2MQTT_NODEDIR)/include && \
	ln -sfn $(ZIGBEE2MQTT_NODE_HEADERS_SRC) $(ZIGBEE2MQTT_NODEDIR)/include/node && \
	ln -sfn $(ZIGBEE2MQTT_NODE_HEADERS_SRC)/common.gypi $(ZIGBEE2MQTT_NODEDIR)/common.gypi && \
	ln -sfn $(ZIGBEE2MQTT_NODE_HEADERS_SRC)/config.gypi $(ZIGBEE2MQTT_NODEDIR)/config.gypi && \
	cd $(1) && \
	CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" \
	AR="$(TARGET_AR)" RANLIB="$(TARGET_RANLIB)" \
	CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" \
	$(ZIGBEE2MQTT_NODE_GYP) rebuild \
		--arch=arm64 \
		--nodedir=$(ZIGBEE2MQTT_NODEDIR)
endef

# Use pnpm for package management
define ZIGBEE2MQTT_BUILD_CMDS
	# Step 1: install pnpm locally
	cd $(@D) && $(NPM) install --no-save pnpm
	# Step 2: patch package.json to point zigbee-herdsman at its build dir
	sed -i 's|"file:\.\./zigbee-herdsman"|"file:$(ZIGBEE_HERDSMAN_BUILD_DIR)"|g' \
		$(@D)/package.json
	# Step 3: install all deps for building (--ignore-scripts avoids x86 native builds)
	cd $(@D) && PATH=$(@D)/node_modules/.bin:$$PATH \
		pnpm install --no-frozen-lockfile --ignore-workspace --ignore-scripts
	# Step 4: build TypeScript
	cd $(@D) && PATH=$(@D)/node_modules/.bin:$$PATH pnpm run build
	# Step 5: reinstall only production deps (removes devDeps)
	cd $(@D) && PATH=$(@D)/node_modules/.bin:$$PATH pnpm install \
		--prod --no-frozen-lockfile --ignore-scripts --ignore-workspace
	# Step 6: cross-compile native addons for aarch64
	$(call CROSS_COMPILE_ADDON, \
		$(@D)/node_modules/.pnpm/unix-dgram@2.0.7/node_modules/unix-dgram)
	# Also handle older version if present
	if [ -d $(@D)/node_modules/.pnpm/unix-dgram@2.0.6/node_modules/unix-dgram ]; then \
		$(call CROSS_COMPILE_ADDON, \
			$(@D)/node_modules/.pnpm/unix-dgram@2.0.6/node_modules/unix-dgram); \
	fi
endef

define ZIGBEE2MQTT_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/opt/zigbee2mqtt
	# Only copy runtime-needed files: entrypoint, built output and production node_modules
	cp -f    $(@D)/index.js        $(TARGET_DIR)/opt/zigbee2mqtt/
	cp -dpfr $(@D)/dist            $(TARGET_DIR)/opt/zigbee2mqtt/
	cp -dpfr $(@D)/node_modules    $(TARGET_DIR)/opt/zigbee2mqtt/
	cp -f    $(@D)/package.json    $(TARGET_DIR)/opt/zigbee2mqtt/
	# Fix zigbee-herdsman symlink to point to runtime path on target
	rm -f  $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/zigbee-herdsman
	ln -sfn /opt/zigbee-herdsman $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/zigbee-herdsman
	rm -f  $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/.pnpm/node_modules/zigbee-herdsman 2>/dev/null || true

	# Create data mountpoint (will be bind-mounted from /mnt/overlay/opt/zigbee2mqtt/data at runtime)
	mkdir -p $(TARGET_DIR)/opt/zigbee2mqtt/data
	mkdir -p $(TARGET_DIR)/opt/zigbee2mqtt/scripts
	
	# Install config templates into the app dir (copied to /mnt/data at first boot)
	mkdir -p $(TARGET_DIR)/opt/zigbee2mqtt/configs
	if [ -f $(ZIGBEE2MQTT_PKGDIR)/configs/configuration_zigate.yaml ]; then \
		$(INSTALL) -D -m 0644 $(ZIGBEE2MQTT_PKGDIR)/configs/configuration_zigate.yaml \
			$(TARGET_DIR)/opt/zigbee2mqtt/configs/configuration_zigate.yaml; \
	fi
	if [ -f $(ZIGBEE2MQTT_PKGDIR)/configs/configuration_blz.yaml ]; then \
		$(INSTALL) -D -m 0644 $(ZIGBEE2MQTT_PKGDIR)/configs/configuration_blz.yaml \
			$(TARGET_DIR)/opt/zigbee2mqtt/configs/configuration_blz.yaml; \
	fi

	# Install external converters into app dir
	mkdir -p $(TARGET_DIR)/opt/zigbee2mqtt/external_converters
	if [ -d $(ZIGBEE2MQTT_PKGDIR)/converters ] && [ -n "$$(ls -A $(ZIGBEE2MQTT_PKGDIR)/converters/*.js 2>/dev/null)" ]; then \
		cp $(ZIGBEE2MQTT_PKGDIR)/converters/*.js \
			$(TARGET_DIR)/opt/zigbee2mqtt/external_converters/ || true; \
	fi
	
	# Install scripts if they exist
	if [ -f $(ZIGBEE2MQTT_PKGDIR)/scripts/zigbee2mqtt_blz_reset.sh ]; then \
		$(INSTALL) -D -m 0755 $(ZIGBEE2MQTT_PKGDIR)/scripts/zigbee2mqtt_blz_reset.sh \
			$(TARGET_DIR)/opt/zigbee2mqtt/scripts/zigbee2mqtt_blz_reset.sh; \
	fi
	if [ -f $(ZIGBEE2MQTT_PKGDIR)/scripts/z2m-permit-on-passlist.sh ]; then \
		$(INSTALL) -D -m 0755 $(ZIGBEE2MQTT_PKGDIR)/scripts/z2m-permit-on-passlist.sh \
			$(TARGET_DIR)/opt/zigbee2mqtt/scripts/z2m-permit-on-passlist.sh; \
	fi
	
	# Install systemd service and mount unit
	$(INSTALL) -D -m 0644 $(ZIGBEE2MQTT_PKGDIR)/zigbee2mqtt.service \
		$(TARGET_DIR)/usr/lib/systemd/system/zigbee2mqtt.service
	$(INSTALL) -D -m 0644 $(ZIGBEE2MQTT_PKGDIR)/opt-zigbee2mqtt-data.mount \
		$(TARGET_DIR)/usr/lib/systemd/system/opt-zigbee2mqtt-data.mount
	mkdir -p $(TARGET_DIR)/usr/lib/systemd/system/multi-user.target.wants
	ln -sf ../opt-zigbee2mqtt-data.mount \
		$(TARGET_DIR)/usr/lib/systemd/system/multi-user.target.wants/opt-zigbee2mqtt-data.mount
	ln -sf ../zigbee2mqtt.service \
		$(TARGET_DIR)/usr/lib/systemd/system/multi-user.target.wants/zigbee2mqtt.service
	# Remove non-aarch64 binaries to pass buildroot arch check
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/.pnpm/esbuild@*/
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/.pnpm/@esbuild+*
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/.pnpm/@rollup+rollup-linux-x64*
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/.pnpm/@rollup+rollup-linux-x64-musl*
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/.pnpm/@biomejs+cli-linux-x64*
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/@rollup/rollup-linux-x64-gnu
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/@esbuild
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/esbuild
	rm -rf $(TARGET_DIR)/opt/zigbee2mqtt/node_modules/@biomejs
	find $(TARGET_DIR)/opt/zigbee2mqtt/node_modules -path "*/prebuilds/linux-x64" -type d \
		-exec rm -rf {} + 2>/dev/null || true
	find $(TARGET_DIR)/opt/zigbee2mqtt/node_modules -path "*/prebuilds/linux-arm" -type d \
		-exec rm -rf {} + 2>/dev/null || true
	find $(TARGET_DIR)/opt/zigbee2mqtt/node_modules -path "*/prebuilds/android-arm" -type d \
		-exec rm -rf {} + 2>/dev/null || true
	find $(TARGET_DIR)/opt/zigbee2mqtt/node_modules -path "*/.ignored_bindings-cpp" -type d \
		-exec rm -rf {} + 2>/dev/null || true
	# Remove intermediate build objects (obj.target dirs with .o files)
	find $(TARGET_DIR)/opt/zigbee2mqtt/node_modules -name "obj.target" -type d \
		-exec rm -rf {} + 2>/dev/null || true
endef

$(eval $(generic-package))
