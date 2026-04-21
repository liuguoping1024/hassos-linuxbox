#!/bin/bash
#
# Zigbee2MQTT BLZ Reset Script (HubV3A)
# GPIO pins: A113X_RST_ZG = GPIOA_17 (43), Z_ISP = GPIOA_16 (42)
#

set -e

LOG_FILE="/var/log/zigbee2mqtt_blz_reset.log"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

check_gpio_tools() {
    if ! command -v gpioset &> /dev/null; then
        log "ERROR: gpioset command not found. Please install gpiod tools:"
        log "  sudo apt-get install gpiod"
        exit 1
    fi
}

gpio_reset_sequence() {
    log "[HubV3A] Starting Zigbee GPIO reset sequence..."

    # GPIOA_17 (pin 43) = reset (active LOW)
    # GPIOA_16 (pin 42) = ISP/boot (LOW = normal mode, HIGH = bootloader)
    # Keep ISP LOW to boot in normal Zigbee mode (not bootloader)

     # GPIO 0:42 = 0 (boot pin high)
    gpioset 0 42=1
    sleep 0.2

    # GPIO 0:42 = 0 (boot pin low)
    gpioset 0 42=0
    sleep 0.2
    
    # GPIO 0:43 = 1 (reset pin high)
    gpioset 0 43=1
    sleep 0.2
    
    # GPIO 0:43 = 0 (reset pin low)
    gpioset 0 43=0
    sleep 0.2
    
    # GPIO 0:43 = 1 (reset pin high)
    gpioset 0 43=1
    sleep 0.5

    log "[HubV3A] Zigbee GPIO reset sequence completed"
}

log "Starting Zigbee2MQTT BLZ reset process..."

check_gpio_tools

gpio_reset_sequence

log "[HubV3A] Zigbee2MQTT BLZ reset process completed"
