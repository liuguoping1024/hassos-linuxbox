#!/bin/bash
# shellcheck disable=SC2155
#
# HubV3L post-image hook
#
# Produces aml_upgrade_package.img for Amlogic USB Burning Tool.
#
# Inputs (already in BINARIES_DIR from buildroot):
#   - Image.gz          (kernel, from BR2_LINUX_KERNEL)
#   - axg_s420_1g.dtb   (DTB, from BR2_LINUX_KERNEL)
#   - rootfs.erofs       (HAOS rootfs)
#   - u-boot.bin*        (from uboot-legacy package)
#
# Prebuilt blobs (from aml-fip/):
#   - rootfs.cpio.gz     (SDK ramdisk, temporary)
#   - logo.img
#
# Tools (from aml-tools/):
#   - dtbTool, mkbootimg, aml_image_v2_packer_new, aml_upgrade_pkg_gen.sh
#

BOARD_DIR="${BR2_EXTERNAL_HASSOS_PATH}/board/thirdreality/hubv3l"
TOOLS_DIR="${BOARD_DIR}/aml-tools"
FIP_DIR="${BOARD_DIR}/aml-fip"
UPGRADE_DIR="${BOARD_DIR}/upgrade-axg"

# Override create_disk_image - hubv3l uses Amlogic image packaging
function create_disk_image() {
    echo "HubV3L: skipping genimage (using Amlogic packaging)"
}

function hassos_pre_image() {
    mkdir -p "$(path_boot_dir)"
}

function hassos_post_image() {
    echo ""
    echo "###########################################################"
    echo "# HubV3L post-image: Amlogic AXG packaging"
    echo "###########################################################"
    echo ""

    _create_dtb_img
    _create_boot_img
    _create_recovery_img
    _prepare_system_partition
    _copy_upgrade_templates
    _pack_upgrade_image
}

# ---------------------------------------------------------------------------
# 1) dtb.img - multi-DTB archive (dtbTool + gzip)
# ---------------------------------------------------------------------------
function _create_dtb_img() {
    local dtb_dir="${BINARIES_DIR}/dtb-input"

    echo "=== Creating dtb.img ==="
    rm -rf "${dtb_dir}"
    mkdir -p "${dtb_dir}"

    # Copy all DTBs from kernel build
    cp -f "${BINARIES_DIR}"/*.dtb "${dtb_dir}/" 2>/dev/null || true

    [ "$(ls -A "${dtb_dir}")" ] || { echo "ERROR: no .dtb files found"; exit 1; }

    "${TOOLS_DIR}/dtbTool" -o "${BINARIES_DIR}/dtb.img" \
        -p "${BUILD_DIR}/linux-5.4.180/scripts/dtc/" "${dtb_dir}/"

    gzip -f "${BINARIES_DIR}/dtb.img"
    mv "${BINARIES_DIR}/dtb.img.gz" "${BINARIES_DIR}/dtb.img"
    echo "    -> $(ls -la "${BINARIES_DIR}/dtb.img")"
}

# ---------------------------------------------------------------------------
# 2) boot.img - mkbootimg (kernel + ramdisk + dtb, Android header)
# ---------------------------------------------------------------------------
function _create_boot_img() {
    local kernel="${BINARIES_DIR}/Image.gz"
    local ramdisk="${FIP_DIR}/rootfs.cpio.gz"
    local dtb="${BINARIES_DIR}/dtb.img"

    [ -f "${kernel}" ]  || { echo "ERROR: ${kernel} not found"; exit 1; }
    [ -f "${ramdisk}" ] || { echo "ERROR: ${ramdisk} not found"; exit 1; }
    [ -f "${dtb}" ]     || { echo "ERROR: ${dtb} not found"; exit 1; }

    echo "=== Creating boot.img ==="
    "${TOOLS_DIR}/mkbootimg" \
        --kernel   "${kernel}" \
        --base     0x0 \
        --kernel_offset 0x1080000 \
        --cmdline  "root=/dev/system rootfstype=erofs ro rootwait init=/sbin/init console=ttyS0,115200n8 zram.num_devices=3" \
        --ramdisk  "${ramdisk}" \
        --second   "${dtb}" \
        --output   "${BINARIES_DIR}/boot.img"
    echo "    -> $(ls -la "${BINARIES_DIR}/boot.img")"
}

# ---------------------------------------------------------------------------
# 3) recovery.img - reuse boot.img for now
# ---------------------------------------------------------------------------
function _create_recovery_img() {
    echo "=== Creating recovery.img (copy of boot.img) ==="
    cp -f "${BINARIES_DIR}/boot.img" "${BINARIES_DIR}/recovery.img"
}

# ---------------------------------------------------------------------------
# 4) system partition: rootfs.erofs -> rootfs.ubi
# ---------------------------------------------------------------------------
function _prepare_system_partition() {
    local haos_rootfs="${BINARIES_DIR}/rootfs.erofs"
    [ -f "${haos_rootfs}" ] || { echo "ERROR: ${haos_rootfs} not found"; exit 1; }

    echo "=== Preparing system partition (erofs -> rootfs.ubi) ==="
    cp -f "${haos_rootfs}" "${BINARIES_DIR}/rootfs.ubi"
    # Keep rootfs.ubifs so aml_upgrade_pkg_gen.sh takes the NAND branch
    cp -f "${haos_rootfs}" "${BINARIES_DIR}/rootfs.ubifs"
    echo "    rootfs.ubi: $(stat -c %s "${BINARIES_DIR}/rootfs.ubi") bytes (erofs)"
}

# ---------------------------------------------------------------------------
# 5) Copy upgrade templates + prebuilt blobs
# ---------------------------------------------------------------------------
function _copy_upgrade_templates() {
    echo "=== Copying upgrade templates ==="
    cp -f "${UPGRADE_DIR}"/* "${BINARIES_DIR}/"
    cp -f "${FIP_DIR}/logo.img" "${BINARIES_DIR}/" 2>/dev/null || true
}

# ---------------------------------------------------------------------------
# 6) Pack aml_upgrade_package.img
# ---------------------------------------------------------------------------
function _pack_upgrade_image() {
    local out="${BINARIES_DIR}/aml_upgrade_package.img"

    echo "=== Packing aml_upgrade_package.img ==="
    BINARIES_DIR="${BINARIES_DIR}" TOOL_DIR="${TOOLS_DIR}" \
        "${TOOLS_DIR}/aml_upgrade_pkg_gen.sh" "axg" "" ""

    [ -f "${out}" ] || { echo "ERROR: packer produced no ${out}"; exit 1; }

    echo ""
    echo "###########################################################"
    echo "# Flashable image ready:"
    echo "#   ${out} ($(stat -c %s "${out}") bytes)"
    echo "# Burn with Amlogic USB Burning Tool"
    echo "###########################################################"
}
