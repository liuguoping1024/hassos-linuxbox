#!/bin/bash
#
# FIP packaging for Amlogic A113X (AXG)
# Called by uboot-legacy.mk after BL33 compilation.
#
# Usage: build-fip.sh <bl33-build-dir> <fip-dir> <board-name> <output-dir>
#
set -e

# Bare-metal aarch64-elf toolchain (Amlogic u-boot 2015.01 / BL30 / BL301).
# Location is overridable so this is not tied to one workstation; the default
# matches the historical hardcoded path.
AML_BAREMETAL_TOOLCHAIN="${AML_BAREMETAL_TOOLCHAIN:-/opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-elf}"
if [ ! -x "${AML_BAREMETAL_TOOLCHAIN}/bin/aarch64-elf-gcc" ]; then
    echo "ERROR: bare-metal toolchain not found." >&2
    echo "  looked for: ${AML_BAREMETAL_TOOLCHAIN}/bin/aarch64-elf-gcc" >&2
    echo "  set AML_BAREMETAL_TOOLCHAIN=/path/to/gcc-linaro-<ver>-aarch64-elf" >&2
    exit 1
fi
export PATH="${AML_BAREMETAL_TOOLCHAIN}/bin:${PATH}"

BL33_DIR="$1"       # u-boot source/build dir (contains build/)
FIP_BASE="$2"       # aml-fip/ dir (contains bl2/, bl30/, bl31_1.3/, fip/)
BOARD="$3"           # e.g. axg_s420_v1
OUTPUT_DIR="$4"      # BINARIES_DIR

BUILD_DIR="${BL33_DIR}/build"
FIP_TOOLS="${FIP_BASE}/fip"
CUR_SOC="axg"

echo "=== FIP Packaging ==="
echo "  BL33:   ${BL33_DIR}"
echo "  FIP:    ${FIP_BASE}"
echo "  Board:  ${BOARD}"
echo "  Output: ${OUTPUT_DIR}"

# Temp dir
FIP_TMP="${BUILD_DIR}/_fip_tmp"
rm -rf "${FIP_TMP}"
mkdir -p "${FIP_TMP}"

# Copy prebuilt BL binaries
cp "${FIP_BASE}/bl2/bin/${CUR_SOC}/bl2.bin"       "${FIP_TMP}/bl2.bin"
cp "${FIP_BASE}/bl30/bin/${CUR_SOC}/bl30.bin"      "${FIP_TMP}/bl30.bin"
cp "${FIP_BASE}/bl31_1.3/bin/${CUR_SOC}/bl31.img"  "${FIP_TMP}/bl31.img"

# Copy BL33 build products
cp "${BUILD_DIR}/u-boot.bin"                                     "${FIP_TMP}/bl33.bin"
cp "${BUILD_DIR}/board/amlogic/${BOARD}/firmware/acs.bin"         "${FIP_TMP}/acs.bin"
cp "${BUILD_DIR}/board/amlogic/${BOARD}/firmware/bl21.bin"        "${FIP_TMP}/bl21.bin"

# bl301 (may not exist if arm-none-eabi-gcc is missing)
if [ -f "${BUILD_DIR}/scp_task/bl301.bin" ]; then
    cp "${BUILD_DIR}/scp_task/bl301.bin" "${FIP_TMP}/bl301.bin"
    echo "  bl301.bin: from build"
elif [ -f "${FIP_BASE}/bl301/bin/${CUR_SOC}/bl301.bin" ]; then
    cp "${FIP_BASE}/bl301/bin/${CUR_SOC}/bl301.bin" "${FIP_TMP}/bl301.bin"
    echo "  bl301.bin: prebuilt fallback ($(stat -c%s ${FIP_TMP}/bl301.bin) bytes)"
else
    echo "  WARNING: bl301.bin not found, using empty placeholder"
fi

#--- fix_blx: BL30 + BL301 ---
BL30_LIMIT=40960
BL301_LIMIT=13312

bl30_size=$(stat -c%s "${FIP_TMP}/bl30.bin")
bl30_zero=$((BL30_LIMIT - bl30_size))
dd if=/dev/zero of="${FIP_TMP}/zero_tmp" bs=1 count=${bl30_zero} 2>/dev/null
cat "${FIP_TMP}/bl30.bin" "${FIP_TMP}/zero_tmp" > "${FIP_TMP}/bl30_zero.bin"

if [ -f "${FIP_TMP}/bl301.bin" ]; then
    bl301_size=$(stat -c%s "${FIP_TMP}/bl301.bin")
    bl301_zero=$((BL301_LIMIT - bl301_size))
    dd if=/dev/zero of="${FIP_TMP}/zero_tmp" bs=1 count=${bl301_zero} 2>/dev/null
    cat "${FIP_TMP}/bl301.bin" "${FIP_TMP}/zero_tmp" > "${FIP_TMP}/bl301_zero.bin"
else
    dd if=/dev/zero of="${FIP_TMP}/bl301_zero.bin" bs=1 count=${BL301_LIMIT} 2>/dev/null
fi

cat "${FIP_TMP}/bl30_zero.bin" "${FIP_TMP}/bl301_zero.bin" > "${FIP_TMP}/bl30_new.bin"
rm -f "${FIP_TMP}/zero_tmp"

#--- acs_tool: inject DDR params into BL2 ---
# The SDK's acs_tool.pyc is Python 2.7 bytecode; use the Python 3 port
# (decompiled and ported by the SDK team) instead of requiring python2.
python3 "${FIP_TOOLS}/acs_tool.py" \
    "${FIP_TMP}/bl2.bin" "${FIP_TMP}/bl2_acs.bin" "${FIP_TMP}/acs.bin" 0

#--- fix_blx: BL2 + BL21 ---
BL2_LIMIT=41984
BL21_LIMIT=7168

bl2_size=$(stat -c%s "${FIP_TMP}/bl2_acs.bin")
bl2_zero=$((BL2_LIMIT - bl2_size))
dd if=/dev/zero of="${FIP_TMP}/zero_tmp" bs=1 count=${bl2_zero} 2>/dev/null
cat "${FIP_TMP}/bl2_acs.bin" "${FIP_TMP}/zero_tmp" > "${FIP_TMP}/bl2_zero.bin"

bl21_size=$(stat -c%s "${FIP_TMP}/bl21.bin")
bl21_zero=$((BL21_LIMIT - bl21_size))
dd if=/dev/zero of="${FIP_TMP}/zero_tmp" bs=1 count=${bl21_zero} 2>/dev/null
cat "${FIP_TMP}/bl21.bin" "${FIP_TMP}/zero_tmp" > "${FIP_TMP}/bl21_zero.bin"

cat "${FIP_TMP}/bl2_zero.bin" "${FIP_TMP}/bl21_zero.bin" > "${FIP_TMP}/bl2_new.bin"
rm -f "${FIP_TMP}/zero_tmp"

#--- fip_create ---
"${FIP_TOOLS}/fip_create" \
    --bl30 "${FIP_TMP}/bl30_new.bin" \
    --bl31 "${FIP_TMP}/bl31.img" \
    --bl33 "${FIP_TMP}/bl33.bin" \
    "${FIP_TMP}/fip.bin"

cat "${FIP_TMP}/bl2_new.bin" "${FIP_TMP}/fip.bin" > "${FIP_TMP}/boot_new.bin"

#--- aml_encrypt_axg ---
AML_ENCRYPT="${FIP_TOOLS}/${CUR_SOC}/aml_encrypt_${CUR_SOC}"

AUTOCONF="${BUILD_DIR}/include/autoconf.mk"
CONFIG_AML_SECURE_BOOT_V3="n"
CONFIG_AML_CRYPTO_UBOOT="n"
CONFIG_AML_BL33_COMPRESS_ENABLE="n"
grep -q 'CONFIG_AML_SECURE_BOOT_V3=y' "${AUTOCONF}" 2>/dev/null && CONFIG_AML_SECURE_BOOT_V3="y"
grep -q 'CONFIG_AML_CRYPTO_UBOOT=y' "${AUTOCONF}" 2>/dev/null && CONFIG_AML_CRYPTO_UBOOT="y"
grep -q 'CONFIG_AML_BL33_COMPRESS_ENABLE=y' "${AUTOCONF}" 2>/dev/null && CONFIG_AML_BL33_COMPRESS_ENABLE="y"

BL33_COMPRESS_FLAG=""
if [ "${CONFIG_AML_BL33_COMPRESS_ENABLE}" = "y" ]; then
    BL33_COMPRESS_FLAG="--compress lz4"
    echo "  BL33 compression: lz4"
fi

if [ "${CONFIG_AML_SECURE_BOOT_V3}" = "y" ]; then
    "${AML_ENCRYPT}" --bl3sig --input "${FIP_TMP}/bl30_new.bin" --output "${FIP_TMP}/bl30_new.bin.enc" --level v3 --type bl30
    "${AML_ENCRYPT}" --bl3sig --input "${FIP_TMP}/bl31.img"     --output "${FIP_TMP}/bl31.img.enc"     --level v3 --type bl31
    "${AML_ENCRYPT}" --bl3sig --input "${FIP_TMP}/bl33.bin"     ${BL33_COMPRESS_FLAG} --output "${FIP_TMP}/bl33.bin.enc" --level v3 --type bl33
    V3_FLAG="--level v3"
else
    "${AML_ENCRYPT}" --bl3enc --input "${FIP_TMP}/bl30_new.bin" --output "${FIP_TMP}/bl30_new.bin.enc"
    "${AML_ENCRYPT}" --bl3enc --input "${FIP_TMP}/bl31.img"     --output "${FIP_TMP}/bl31.img.enc"
    "${AML_ENCRYPT}" --bl3enc --input "${FIP_TMP}/bl33.bin"     --output "${FIP_TMP}/bl33.bin.enc"     ${BL33_COMPRESS_FLAG}
    V3_FLAG=""
fi

"${AML_ENCRYPT}" --bl2sig --input "${FIP_TMP}/bl2_new.bin" --output "${FIP_TMP}/bl2.n.bin.sig"

"${AML_ENCRYPT}" --bootmk --output "${FIP_TMP}/u-boot.bin" \
    --bl2  "${FIP_TMP}/bl2.n.bin.sig" \
    --bl30 "${FIP_TMP}/bl30_new.bin.enc" \
    --bl31 "${FIP_TMP}/bl31.img.enc" \
    --bl33 "${FIP_TMP}/bl33.bin.enc" ${V3_FLAG}

# Secure boot encryption (if enabled)
if [ "${CONFIG_AML_CRYPTO_UBOOT}" = "y" ]; then
    AML_KEY="${FIP_BASE}/binary/board/amlogic/${BOARD}/aml-user-key.sig"
    "${AML_ENCRYPT}" --efsgen --amluserkey "${AML_KEY}" \
        --output "${FIP_TMP}/u-boot.bin.encrypt.efuse" ${V3_FLAG}
    "${AML_ENCRYPT}" --bootsig --input "${FIP_TMP}/u-boot.bin" \
        --amluserkey "${AML_KEY}" --aeskey enable \
        --output "${FIP_TMP}/u-boot.bin.encrypt" ${V3_FLAG}
fi

#--- Copy outputs to BINARIES_DIR ---
echo "=== Installing u-boot to ${OUTPUT_DIR} ==="
cp "${FIP_TMP}"/u-boot.bin* "${OUTPUT_DIR}/"
echo "  Done: $(ls ${OUTPUT_DIR}/u-boot.bin*)"
