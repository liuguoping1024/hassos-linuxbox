#!/bin/bash
set -e

current_dir=$(pwd)

# --- Parse arguments ---
BOARD="hubv3"
DO_CLEAN=false

usage() {
    echo "Usage: $0 [-b hubv3|hubv3a|hubv3b] [clean]"
    echo ""
    echo "Options:"
    echo "  -b BOARD   Board variant (default: hubv3)"
    echo "             hubv3  - base model, no zigbee2mqtt"
    echo "             hubv3b - same hardware as hubv3, with zigbee2mqtt"
    echo "             hubv3a - cost-reduced variant, with zigbee2mqtt"
    echo "  clean      Remove output directory before building"
    echo ""
    echo "Examples:"
    echo "  $0                    # build hubv3"
    echo "  $0 -b hubv3b         # build hubv3b"
    echo "  $0 -b hubv3a clean   # clean + build hubv3a"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b)
            BOARD="$2"
            shift 2
            ;;
        clean)
            DO_CLEAN=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            ;;
    esac
done

case "${BOARD}" in
    hubv3)  DEFCONFIG="thirdreality_hubv3_defconfig"  ;;
    hubv3a) DEFCONFIG="thirdreality_hubv3a_defconfig" ;;
    hubv3b) DEFCONFIG="thirdreality_hubv3b_defconfig" ;;
    hubv3x) DEFCONFIG="thirdreality_hubv3x_defconfig" ;;
    *)
        echo "Error: unknown board '${BOARD}', must be hubv3|hubv3a|hubv3b|hubv3x"
        exit 1
        ;;
esac

NEEDS_NODEJS=false
case "${BOARD}" in
    hubv3a|hubv3b|hubv3x) NEEDS_NODEJS=true ;;
esac

echo "========================================"
echo "  Board:    ${BOARD}"
echo "  Defconfig: ${DEFCONFIG}"
echo "  Node.js:  ${NEEDS_NODEJS}"
echo "  Clean:    ${DO_CLEAN}"
echo "  Work dir: ${current_dir}"
echo "========================================"

# --- Buildroot submodule initialization ---
subdir_count=0
if [ -d "${current_dir}/buildroot" ]; then
    subdir_count=$(find ${current_dir}/buildroot -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l)
fi

if [ ! -d "${current_dir}/buildroot" ] || [ "$subdir_count" -lt 2 ]; then
    mkdir -p ${current_dir}/buildroot
    echo "Buildroot directory missing or incomplete (subdirs: $subdir_count), initializing..."
    /usr/bin/git pull && /usr/bin/git submodule update --init --recursive
    /usr/bin/git submodule sync
    echo "Buildroot initialization completed!"
else
    echo "Buildroot directory exists and is complete (subdirs: $subdir_count), skipping sync"
fi

# --- Apply buildroot patches ---
# These patches modify the buildroot submodule (python3, nodejs, etc.)
# They are idempotent: already-applied patches are skipped automatically.
PATCH_DIR="${current_dir}/buildroot-external/patches/buildroot"

if [ -d "${PATCH_DIR}" ]; then
    echo ""
    echo "--- Applying buildroot patches ---"
    for patch_file in "${PATCH_DIR}"/*.patch; do
        [ -f "$patch_file" ] || continue
        patch_name=$(basename "$patch_file")
        if cd "${current_dir}/buildroot" && git apply --check "$patch_file" 2>/dev/null; then
            echo "Applying: ${patch_name}"
            git apply "$patch_file"
            echo "  Applied successfully"
        else
            echo "Skipping: ${patch_name} (already applied or conflict)"
        fi
        cd "${current_dir}"
    done
else
    echo ""
    echo "--- No buildroot patches found, skipping ---"
fi

# --- Prepare directories ---
mkdir -p /cache
mkdir -p /build

# --- Clean ---
if [ "${DO_CLEAN}" = true ]; then
    echo ""
    echo "--- Cleaning output directory ---"
    rm -rf "${current_dir}/output"
    echo "Output directory removed"
else
    echo ""
    echo "--- Incremental build: cleaning target packages only ---"
    find ${current_dir}/output/build -maxdepth 1 -type d ! -name "host-*" ! -name "build" -exec rm -rf {} + 2>/dev/null || true
    rm -rf ${current_dir}/output/build/host-uboot-tools-* > /dev/null 2>&1 || true
    find ${current_dir}/output/build -maxdepth 1 -type f -exec rm -f {} + 2>/dev/null || true
    rm -rf ${current_dir}/output/target > /dev/null 2>&1 || true
    rm -rf ${current_dir}/output/images > /dev/null 2>&1 || true
fi

# --- Configure ---
echo ""
echo "Configure buildroot for ${BOARD} (${DEFCONFIG})"
/usr/bin/make -C ${current_dir}/buildroot O=${current_dir}/output \
    BR2_EXTERNAL=${current_dir}/buildroot-external "${DEFCONFIG}"

# --- Build ---
echo ""
echo "Build ${BOARD}"
/usr/bin/make -C ${current_dir}/buildroot O=${current_dir}/output \
    BR2_EXTERNAL=${current_dir}/buildroot-external
