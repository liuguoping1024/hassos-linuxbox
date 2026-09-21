#!/bin/bash
set -e


# Directory holding the RAUC signing key/cert.
#
# This used to be hardcoded as "/build", which is the WORKDIR of the build
# container (see Dockerfile) — i.e. inside CI it simply means "the repo root",
# because that is where the checkout lives and where the workflow drops
# cert.pem / key.pem. Deriving it from BR2_EXTERNAL_HASSOS_PATH resolves to
# exactly the same /build/*.pem inside the container while pointing at the
# actual repo root for native builds, so no root-owned /build is needed.
#
# Override with RAUC_KEY_DIR if the key lives elsewhere.
function rauc_key_dir() {
    echo "${RAUC_KEY_DIR:-$(dirname "${BR2_EXTERNAL_HASSOS_PATH}")}"
}


function prepare_rauc_signing() {
    local dir key cert
    dir="$(rauc_key_dir)"
    key="${dir}/key.pem"
    cert="${dir}/cert.pem"

    if [ ! -f "${key}" ]; then
        echo "Generating a self-signed certificate for development"
        "${BR2_EXTERNAL_HASSOS_PATH}"/scripts/generate-signing-key.sh "${cert}" "${key}"
        return
    fi

    # An existing but unreadable key used to surface much later as
    # "rauc bundle: failed to load key file", after the whole image had
    # already been assembled. Fail here instead, where the cause is obvious.
    if [ ! -r "${key}" ] || [ ! -r "${cert}" ]; then
        echo "ERROR: RAUC signing key/cert exist but are not readable by $(id -un):" >&2
        ls -l "${key}" "${cert}" >&2 2>/dev/null || true
        echo "  fix the ownership, or point RAUC_KEY_DIR at a readable copy," >&2
        echo "  or remove them to have a fresh self-signed dev cert generated." >&2
        exit 1
    fi
}


function write_rauc_config() {
    mkdir -p "${TARGET_DIR}/etc/rauc"

    local ota_compatible
    ota_compatible="$(hassos_rauc_compatible)"

    export ota_compatible
    export BOOTLOADER PARTITION_TABLE_TYPE BOOT_SPL

    (
        "${HOST_DIR}/bin/tempio" \
            -template "${BR2_EXTERNAL_HASSOS_PATH}/ota/system.conf.gtpl"
    ) > "${TARGET_DIR}/etc/rauc/system.conf"
}


function install_rauc_certs() {
    local cert
    cert="$(rauc_key_dir)/cert.pem"

    if [ "${DEPLOYMENT}" == "development" ]; then
        # Contains development and release certificate
        cp "${BR2_EXTERNAL_HASSOS_PATH}/ota/dev-ca.pem" "${TARGET_DIR}/etc/rauc/keyring.pem"
    else
        cp "${BR2_EXTERNAL_HASSOS_PATH}/ota/rel-ca.pem" "${TARGET_DIR}/etc/rauc/keyring.pem"
    fi

    # Add local self-signed certificate (if not trusted by the dev or release
    # certificate it is a self-signed certificate, dev-ca.pem contains both)
    if ! openssl verify -CAfile "${BR2_EXTERNAL_HASSOS_PATH}/ota/dev-ca.pem" -no-CApath "${cert}"; then
        echo "Adding self-signed certificate to keyring."
        openssl x509 -in "${cert}" -text >> "${TARGET_DIR}/etc/rauc/keyring.pem"
    fi
}


function install_bootloader_config() {
    if [ "${BOOTLOADER}" == "uboot" ]; then
        # shellcheck disable=SC1117
        echo -e "/dev/disk/by-partlabel/hassos-bootstate\t0x0000\t${BOOT_ENV_SIZE}" > "${TARGET_DIR}/etc/fw_env.config"
    fi

    # Fix MBR
    if [ "${PARTITION_TABLE_TYPE}" == "mbr" ]; then
        mkdir -p "${TARGET_DIR}/usr/lib/udev/rules.d"
        cp -f "${BR2_EXTERNAL_HASSOS_PATH}/bootloader/mbr-part.rules" "${TARGET_DIR}/usr/lib/udev/rules.d/"
    fi
}
