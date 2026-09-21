################################################################################
#
# uboot-legacy - Amlogic A113X (AXG) u-boot v2015.01 + FIP
#
################################################################################

UBOOT_LEGACY_VERSION = v2015.01
UBOOT_LEGACY_SITE = https://github.com/u-boot/u-boot.git
UBOOT_LEGACY_SITE_METHOD = git
UBOOT_LEGACY_LICENSE = GPL-2.0+
UBOOT_LEGACY_LICENSE_FILES = COPYING

UBOOT_LEGACY_BOARD = axg_hubv3l_v1
UBOOT_LEGACY_BOARD_DIR = $(BR2_EXTERNAL_HASSOS_PATH)/board/thirdreality/hubv3l
UBOOT_LEGACY_FIP_DIR = $(UBOOT_LEGACY_BOARD_DIR)/aml-fip

# Install produced images (u-boot.bin*) to BINARIES_DIR via
# UBOOT_LEGACY_INSTALL_IMAGES_CMDS (runs build-fip.sh).
UBOOT_LEGACY_INSTALL_IMAGES = YES

# Toolchain: Amlogic u-boot 2015.01 requires bare-metal aarch64-elf-gcc
# (not the buildroot linux-gnu cross toolchain). Override the location with
# the AML_BAREMETAL_TOOLCHAIN environment variable; the default matches the
# historical hardcoded path.
UBOOT_LEGACY_CROSS = aarch64-elf-
UBOOT_LEGACY_BAREMETAL = $(if $(AML_BAREMETAL_TOOLCHAIN),$(AML_BAREMETAL_TOOLCHAIN),/opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-elf)
UBOOT_LEGACY_BAREMETAL_BIN = $(UBOOT_LEGACY_BAREMETAL)/bin

# Fail loudly and early if the bare-metal toolchain is missing. Without this
# the missing compiler is masked by the "|| true" in BUILD_CMDS and only
# surfaces much later as a confusing "BL33 u-boot.bin not built".
define UBOOT_LEGACY_CHECK_BAREMETAL
	test -x "$(UBOOT_LEGACY_BAREMETAL_BIN)/aarch64-elf-gcc" || { \
		echo "ERROR: bare-metal toolchain not found."; \
		echo "  looked for: $(UBOOT_LEGACY_BAREMETAL_BIN)/aarch64-elf-gcc"; \
		echo "  set AML_BAREMETAL_TOOLCHAIN=/path/to/gcc-linaro-<ver>-aarch64-elf"; \
		exit 1; \
	}
endef

# Patches are applied automatically by buildroot via BR2_GLOBAL_PATCH_DIR
# -> patches/uboot-legacy/ (51 SDK patches + 1 hubv3l board patch)

# Amlogic u-boot uses make <board>_config, not make <board>_defconfig
define UBOOT_LEGACY_CONFIGURE_CMDS
	$(UBOOT_LEGACY_CHECK_BAREMETAL)
	PATH="$(UBOOT_LEGACY_BAREMETAL_BIN):$(PATH)" \
	$(MAKE) -C $(@D) $(UBOOT_LEGACY_BOARD)_config
endef

define UBOOT_LEGACY_BUILD_CMDS
	PATH="$(UBOOT_LEGACY_BAREMETAL_BIN):$(PATH)" \
	$(MAKE) -C $(@D) -j$(PARALLEL_JOBS) \
		CROSS_COMPILE=$(UBOOT_LEGACY_CROSS) \
		SYSTEMMODE=null AVBMODE=null BOOTCTRLMODE=null \
		FASTBOOTMODE=null AVB2RECOVERY=null \
	|| true
	# Amlogic u-boot 2015.01 always fails on bl301 when arm-none-eabi-gcc
	# is missing. This is expected (SDK's build_uboot.sh does the same).
	# Verify the BL33 output we actually need is present.
	test -f $(@D)/build/u-boot.bin || { echo "ERROR: BL33 u-boot.bin not built"; exit 1; }
endef

# FIP packaging: bl2+bl30+bl31+bl33 -> u-boot.bin (encrypted/signed)
define UBOOT_LEGACY_INSTALL_IMAGES_CMDS
	AML_BAREMETAL_TOOLCHAIN="$(UBOOT_LEGACY_BAREMETAL)" \
	$(UBOOT_LEGACY_BOARD_DIR)/scripts/build-fip.sh \
		"$(@D)" \
		"$(UBOOT_LEGACY_FIP_DIR)" \
		"$(UBOOT_LEGACY_BOARD)" \
		"$(BINARIES_DIR)"
endef

$(eval $(generic-package))
