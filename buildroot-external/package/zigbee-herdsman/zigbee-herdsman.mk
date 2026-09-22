################################################################################
#
# zigbee-herdsman
#
################################################################################

ZIGBEE_HERDSMAN_VERSION = 3r_blz_10.0.8
ZIGBEE_HERDSMAN_SITE = https://github.com/thirdreality/zigbee-herdsman.git
ZIGBEE_HERDSMAN_SITE_METHOD = git
ZIGBEE_HERDSMAN_LICENSE = MIT
ZIGBEE_HERDSMAN_LICENSE_FILES = LICENSE
ZIGBEE_HERDSMAN_DEPENDENCIES = nodejs host-nodejs

# Use pnpm for package management
define ZIGBEE_HERDSMAN_BUILD_CMDS
	# Step 1: install pnpm locally
	cd $(@D) && $(NPM) install --no-save pnpm
	# Step 2: install all deps (including devDeps) for building
	cd $(@D) && PATH=$(@D)/node_modules/.bin:$$PATH pnpm install --no-frozen-lockfile
	# Step 3: build (tsc)
	cd $(@D) && PATH=$(@D)/node_modules/.bin:$$PATH pnpm run build
	# Step 4: reinstall only production deps (removes devDeps and host-arch binaries)
	cd $(@D) && PATH=$(@D)/node_modules/.bin:$$PATH pnpm install \
		--prod --no-frozen-lockfile --ignore-scripts
endef

define ZIGBEE_HERDSMAN_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/opt/zigbee-herdsman
	# Only copy runtime-needed files: built output and production node_modules
	cp -dpfr $(@D)/dist            $(TARGET_DIR)/opt/zigbee-herdsman/
	cp -dpfr $(@D)/node_modules    $(TARGET_DIR)/opt/zigbee-herdsman/
	cp -f    $(@D)/package.json    $(TARGET_DIR)/opt/zigbee-herdsman/
	# Remove non-aarch64 binaries to pass buildroot arch check
	# 1. devDep tool packages that pnpm keeps in .pnpm store
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/.pnpm/esbuild@*/
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/.pnpm/@esbuild+*
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/.pnpm/@rollup+rollup-linux-x64*
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/.pnpm/@rollup+rollup-linux-x64-musl*
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/.pnpm/@biomejs+cli-linux-x64*
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/@rollup/rollup-linux-x64-gnu
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/@esbuild
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/esbuild
	rm -rf $(TARGET_DIR)/opt/zigbee-herdsman/node_modules/@biomejs
	# 2. serialport multi-platform prebuilds (keep only linux-arm64)
	find $(TARGET_DIR)/opt/zigbee-herdsman/node_modules -path "*/prebuilds/linux-x64" -type d \
		-exec rm -rf {} + 2>/dev/null || true
	find $(TARGET_DIR)/opt/zigbee-herdsman/node_modules -path "*/prebuilds/linux-arm" -type d \
		-exec rm -rf {} + 2>/dev/null || true
	find $(TARGET_DIR)/opt/zigbee-herdsman/node_modules -path "*/prebuilds/android-arm" -type d \
		-exec rm -rf {} + 2>/dev/null || true
	find $(TARGET_DIR)/opt/zigbee-herdsman/node_modules -path "*/.ignored_bindings-cpp" -type d \
		-exec rm -rf {} + 2>/dev/null || true
endef

$(eval $(generic-package))
