#!/bin/bash
# shellcheck disable=SC1090,SC1091
set -e

SCRIPT_DIR=${BR2_EXTERNAL_HASSOS_PATH}/scripts
BOARD_DIR=${2}

. "${BR2_EXTERNAL_HASSOS_PATH}/meta"
. "${BOARD_DIR}/meta"

. "${SCRIPT_DIR}/rootfs-layer.sh"
. "${SCRIPT_DIR}/name.sh"
. "${SCRIPT_DIR}/rauc.sh"


# HassOS tasks
fix_rootfs
install_tini_docker

# Write os-release
# shellcheck disable=SC2153
(
    echo "NAME=\"${HASSOS_NAME}\""
    echo "VERSION=\"$(hassos_version) (${BOARD_NAME})\""
    echo "ID=${HASSOS_ID}"
    echo "VERSION_ID=$(hassos_version)"
    echo "PRETTY_NAME=\"${HASSOS_NAME} $(hassos_version)\""
    echo "CPE_NAME=cpe:2.3:o:home-assistant:${HASSOS_ID}:$(hassos_version):*:${DEPLOYMENT}:*:*:*:${BOARD_ID}:*"
    echo "HOME_URL=https://hass.io/"
    echo "VARIANT=\"${HASSOS_NAME} ${BOARD_NAME}\""
    echo "VARIANT_ID=${BOARD_ID}"
    echo "SUPERVISOR_MACHINE=${SUPERVISOR_MACHINE}"
    echo "SUPERVISOR_ARCH=${SUPERVISOR_ARCH}"
) > "${TARGET_DIR}/usr/lib/os-release"

# Write machine-info
(
    echo "CHASSIS=${CHASSIS}"
    echo "DEPLOYMENT=${DEPLOYMENT}"
) > "${TARGET_DIR}/etc/machine-info"


# Setup RAUC
prepare_rauc_signing
write_rauc_config
install_rauc_certs
install_bootloader_config

# Ensure os-* and raucdb-update scripts are executable (overlay may lose +x)
for f in "${TARGET_DIR}/usr/libexec/os-expand" "${TARGET_DIR}/usr/libexec/os-overlay" \
         "${TARGET_DIR}/usr/libexec/os-persists" "${TARGET_DIR}/usr/libexec/os-swapfile" \
         "${TARGET_DIR}/usr/libexec/os-zram" "${TARGET_DIR}/usr/libexec/raucdb-update" \
         "${TARGET_DIR}/usr/sbin/os-config"; do
	[ -f "$f" ] && chmod +x "$f"
done

# Fix overlay presets
"${HOST_DIR}/bin/systemctl" --root="${TARGET_DIR}" preset-all

# --- Out-of-tree Amlogic W1 modules must be in the image ---------------------
# They live under extra/, which the kernel's modules_install leaves alone
# (it only replaces kernel/). Fail here rather than ship a board with no
# WiFi or Bluetooth.
_krel=$(ls "${TARGET_DIR}/lib/modules" 2>/dev/null | head -1)
_amlmissing=""
grep -q '^BR2_PACKAGE_AML_WIFI=y' "${BR2_CONFIG}" && for _ko in wifi/aml_sdio.ko wifi/vlsicomm.ko; do
	[ -f "${TARGET_DIR}/lib/modules/${_krel}/extra/amlogic/${_ko}" ] || _amlmissing="${_amlmissing} ${_ko}"
done
grep -q '^BR2_PACKAGE_AML_BT=y' "${BR2_CONFIG}" && \
	{ [ -f "${TARGET_DIR}/lib/modules/${_krel}/extra/amlogic/bt/sdio_bt.ko" ] || _amlmissing="${_amlmissing} bt/sdio_bt.ko"; }
if [ -n "${_amlmissing}" ]; then
	echo "post-build: ERROR: Amlogic modules missing from rootfs:${_amlmissing}" >&2
	echo "post-build: run 'make aml-wifi-reinstall aml-bt-reinstall' in the output dir" >&2
	exit 1
fi
# ---------------------------------------------------------------------------
