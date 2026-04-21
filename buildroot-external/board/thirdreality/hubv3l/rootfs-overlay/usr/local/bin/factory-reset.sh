#!/bin/bash

set -e

SCRIPT="HubV3"

function print_info()  { echo -e "\e[1;34m[${SCRIPT}] INFO:\e[0m $1"; }
function print_error() { echo -e "\e[1;31m[${SCRIPT}] ERROR:\e[0m $1"; }

error_handler() {
    local lineno=$1
    echo "Error occurred at line $lineno"
}

trap 'error_handler $LINENO' ERR

update_zigbee2mqtt_config()
{
    local src="/opt/zigbee2mqtt/configs/configuration_blz.yaml"
    local dst="/opt/zigbee2mqtt/data/configuration.yaml"

    if [ ! -f "$src" ]; then
        print_info "Z2M factory config not found: $src, skipping"
        return 0
    fi

    print_info "Stopping zigbee2mqtt and mosquitto services"
    /usr/bin/systemctl stop zigbee2mqtt.service > /dev/null 2>&1 || true
    /usr/bin/systemctl stop mosquitto.service > /dev/null 2>&1 || true

    if [ -f "$dst" ]; then
        local timestamp
        timestamp=$(date +%Y%m%d%H%M%S)
        local backup="${dst%.yaml}_${timestamp}.yaml"
        print_info "Creating backup: $backup"
        cp "$dst" "$backup" || print_error "Failed to create backup, continuing anyway"
    fi

    print_info "Restoring Zigbee2MQTT configuration from $src"
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst" || { print_error "Failed to copy Z2M configuration"; return 1; }

    print_info "Zigbee2MQTT configuration restored successfully"
}

update_bridge_config()
{
    local src="/etc/hubv3-bridge/configuration.yaml"
    local dst="/var/lib/hubv3-bridge/configuration.yaml"

    if [ ! -f "$src" ]; then
        print_info "Bridge factory config not found: $src, skipping"
        return 0
    fi

    print_info "Stopping linuxbox-hubv3-bridge service"
    /usr/bin/systemctl stop linuxbox-hubv3-bridge.service > /dev/null 2>&1 || true

    if [ -f "$dst" ]; then
        local timestamp
        timestamp=$(date +%Y%m%d%H%M%S)
        local backup="${dst%.yaml}_${timestamp}.yaml"
        print_info "Creating backup: $backup"
        cp "$dst" "$backup" || print_error "Failed to create backup, continuing anyway"
    fi

    print_info "Restoring bridge configuration from $src"
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst" || { print_error "Failed to copy bridge configuration"; return 1; }

    print_info "Bridge configuration restored successfully"
}

update_supervisor_config()
{
    local src="/etc/hubv3-supervisor/configuration.yaml"
    local dst="/var/lib/hubv3-supervisor/configuration.yaml"

    if [ ! -f "$src" ]; then
        print_info "Supervisor factory config not found: $src, skipping"
        return 0
    fi

    print_info "Restoring supervisor configuration from $src"
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst" || { print_error "Failed to copy supervisor configuration"; return 1; }

    print_info "Supervisor configuration restored successfully"
}

# ── Main ──────────────────────────────────────────────────────────────────────

echo "System is start to perform factory reset actions. " | wall

if [ -e "/usr/local/bin/supervisor" ]; then
    /usr/local/bin/supervisor led factory_reset
fi

# Stop application services
# /usr/bin/systemctl stop home-assistant.service     > /dev/null 2>&1 || true
/usr/bin/systemctl stop zigbee2mqtt.service        > /dev/null 2>&1 || true
/usr/bin/systemctl stop mosquitto.service          > /dev/null 2>&1 || true
/usr/bin/systemctl stop linuxbox-hubv3-bridge.service > /dev/null 2>&1 || true
# /usr/bin/systemctl stop otbr-agent.service         > /dev/null 2>&1 || true

# Restore factory configurations
update_zigbee2mqtt_config
/usr/bin/sync

update_bridge_config
/usr/bin/sync

update_supervisor_config
/usr/bin/sync

sleep 0.5

# Reset WiFi
if [ -e "/usr/bin/nmcli" ]; then
    nmcli -t -f UUID con show | xargs -I {} nmcli con delete uuid {} 2>/dev/null || true
fi

if [ -e "/etc/wpa_supplicant/wpa_supplicant-nl80211-wlan0.conf" ]; then
    rm -f /etc/wpa_supplicant/wpa_supplicant-nl80211-wlan0.conf
fi

/usr/bin/systemctl daemon-reload || true

if [ -e "/usr/local/bin/supervisor" ]; then
    /usr/local/bin/supervisor led white
fi

echo "Factory reset completed. Rebooting now..." | wall

/usr/bin/sync
sleep 2
/usr/bin/sync

/usr/sbin/reboot
