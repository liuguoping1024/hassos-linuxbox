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

# --- Persist out-of-tree aml-wifi kernel modules across kernel rebuilds -----
# Root cause: whenever `linux` is (re)built, buildroot reinstalls the in-kernel
# modules into $TARGET_DIR/lib/modules/<ver> and wipes the aml-wifi .ko that
# were installed earlier. On incremental builds aml-wifi is skipped (its build
# stamp is present), so its modules end up missing from the rootfs and WiFi
# fails to load at boot ("insmod: ERROR: could not load module").
# This runs on every build, right before the rootfs image is generated, and
# re-syncs the modules (idempotent). Guarded by the .ko existence check, so it
# only affects boards that actually build aml-wifi.
for _amlvmac in "${BUILD_DIR}"/aml-wifi-*/project_w1/vmac; do
	[ -f "${_amlvmac}/aml_sdio.ko" ] || continue
	_krel=$(ls "${TARGET_DIR}/lib/modules" 2>/dev/null | head -1)
	[ -n "${_krel}" ] || continue
	_wifidir="${TARGET_DIR}/lib/modules/${_krel}/kernel/amlogic/wifi"
	mkdir -p "${_wifidir}"
	for _ko in aml_sdio.ko vlsicomm.ko; do
		[ -f "${_amlvmac}/${_ko}" ] && install -m 0644 "${_amlvmac}/${_ko}" "${_wifidir}/${_ko}"
	done
	"${HOST_DIR}/sbin/depmod" -b "${TARGET_DIR}" "${_krel}" 2>/dev/null || true
	echo "post-build: re-synced aml-wifi modules into ${_wifidir}"
done
# ---------------------------------------------------------------------------
