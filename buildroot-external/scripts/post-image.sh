#!/bin/bash
# shellcheck disable=SC1090,SC1091
set -e

SCRIPT_DIR=${BR2_EXTERNAL_HASSOS_PATH}/scripts
BOARD_DIR=${2}
HOOK_FILE=${3}

. "${BR2_EXTERNAL_HASSOS_PATH}/meta"
. "${BOARD_DIR}/meta"

. "${SCRIPT_DIR}/hdd-image.sh"
. "${SCRIPT_DIR}/rootfs-layer.sh"
. "${SCRIPT_DIR}/name.sh"
. "${SCRIPT_DIR}/rauc.sh"
. "${HOOK_FILE}"

# Cleanup
rm -rf "$(path_boot_dir)"
mkdir -p "$(path_boot_dir)"

# Hook pre image build stuff
hassos_pre_image

# Create empty data partition if it doesn't exist (when HASSIO is disabled).
#
# DATA_INITIAL_SIZE, not DATA_SIZE - see the comment in hdd-image.sh. The
# filesystem is grown to fill its partition on first boot.
#
# Cached in images/, so remove data.ext4 by hand after changing
# DATA_INITIAL_SIZE or the stale image is reused.
if [ ! -f "$(path_data_img)" ]; then
    echo "Creating empty ${DATA_INITIAL_SIZE} data.ext4 partition (HASSIO disabled)..."
    data_img="$(path_data_img)"
    rm -f "${data_img}"
    truncate --size="${DATA_INITIAL_SIZE}" "${data_img}"
    # -b/-i are pinned rather than left to mke2fs.conf. At this size mke2fs
    # picks its "small" profile (blocksize 1024, inode_ratio 4096) where the
    # old full-size image landed in "default" (4096/16384), and both are baked
    # into the filesystem for good - growing it later cannot change either.
    # 4096-byte blocks also raise the online-resize ceiling to 64 GiB, which a
    # 32 GB NAND board needs.
    mkfs.ext4 -b 4096 -i 16384 -L "hassos-data" \
        -E lazy_itable_init=0,lazy_journal_init=0 "${data_img}"
    e2fsck -f -p "${data_img}"
fi

# Disk & OTA
create_disk_image

# Hook post image build stuff
hassos_post_image
