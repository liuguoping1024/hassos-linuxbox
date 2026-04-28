# HubV3L SDK Package 分析

本文档分析 A113X SDK (`sdk_A113X_202210`) 中 `package/amlogic/` 下与 HubV3L 相关的 package，
评估每个 package 对 HubV3L 的必要性。

## 评估标准

- **必须** — WiFi/BT/系统基础功能，缺少则硬件不工作
- **推荐** — 有实用价值，建议后续集成
- **不需要** — HubV3L 场景不使用

---

## WiFi / 蓝牙相关

### aml-wifibt (aml-wifi + aml-bt)

| 项目 | 内容 |
|------|------|
| 源码 | `hardware/aml-5.4/wifi/amlogic` (WiFi) / `hardware/aml-5.4/bluetooth/amlogic/aml_bt/sdio_driver_bt` (BT) |
| 产出 | `vlsicomm.ko` + `aml_sdio.ko` (WiFi 驱动) / `sdio_bt.ko` (BT 驱动) + RF 配置文件 + BT firmware |
| 依赖 | linux 内核源码树（内核外模块编译），aml-bt 依赖 aml-wifi 的 Module.symvers |
| 必要性 | **必须** |
| 说明 | Amlogic W1 自研 WiFi/BT 芯片的 SDIO 驱动。没有这两个 .ko，WiFi 和蓝牙完全无法工作。建议通过 kernel patch 方式将源码加入内核树，随内核一起编译，避免 vermagic 不匹配问题。 |

### wifi-fw

| 项目 | 内容 |
|------|------|
| 源码 | `hardware/aml-4.9/amlogic/wifi` |
| 产出 | BCM/RTK/Cypress/QCA 等外挂 WiFi 芯片的 firmware 文件 (`/etc/wifi/`, `/etc/bluetooth/`) |
| 依赖 | 无 |
| 必要性 | **不需要** |
| 说明 | 这是 Broadcom AP6xxx、Realtek RTL8xxx、Cypress CYW 等第三方 WiFi 芯片的 firmware 包。HubV3L 使用 Amlogic 自研 W1 WiFi，不需要这些外挂芯片的 firmware。aml-wifi 自带的 `aml_wifi_rf*.txt` 已经包含了 W1 的 RF 校准数据。 |

### dhd_priv

| 项目 | 内容 |
|------|------|
| 源码 | `hardware/aml-4.9/amlogic/wifi/bcm_ampak/tools/dhd_priv` |
| 产出 | `dhd_priv` 命令行工具 |
| 依赖 | 无 |
| 必要性 | **不需要** |
| 说明 | Broadcom DHD (Dongle Host Driver) 的私有 ioctl 工具，仅用于 BCM WiFi 芯片调试。HubV3L 不使用 BCM WiFi。 |

### bt-setup

| 项目 | 内容 |
|------|------|
| 源码 | `package/amlogic/bt-setup/src` |
| 产出 | `bt_con.cgi` + BT 配置 Web CGI |
| 依赖 | cjson, brcm-bsa |
| 必要性 | **不需要** |
| 说明 | Broadcom BSA (Bluetooth Server Application) 的 Web 配置界面。仅用于 BCM BT 芯片。HubV3L 使用 Amlogic 自研 BT，通过 bluez5-utils + aml-bt-ct 管理。 |

### bluez5-utils (SDK 定制版)

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/bluez/bluez5_utils` (基于 bluez 5.49) |
| 产出 | bluetoothd, bluetoothctl, hciconfig 等标准 BlueZ 工具 |
| 依赖 | dbus, libglib2 |
| 必要性 | **不需要（已有替代）** |
| 说明 | SDK 自带的 BlueZ 5.49 定制版。HubV3L 的 defconfig 已经启用了 buildroot 上游的 `BR2_PACKAGE_BLUEZ5_UTILS`，版本更新且功能更完整。不需要 SDK 的定制版。 |

### web-ui-wifi

| 项目 | 内容 |
|------|------|
| 源码 | `package/amlogic/web-ui-wifi/src` |
| 产出 | Web UI 页面 + CGI 脚本 (`/var/www/`) |
| 依赖 | 无 |
| 必要性 | **不需要** |
| 说明 | WiFi 配置的 Web 管理界面（HTML + CGI）。HubV3L 使用 NetworkManager + wpa_supplicant 管理 WiFi，不需要 Web UI。 |

---

## 系统工具

### aml-util

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/aml_commonlib/utils` |
| 产出 | WiFi/BT 模块加载工具 (`wifiutil`)、`simulate_key`、电源管理工具等 |
| 依赖 | linux, libusb, aml-commonlib |
| 必要性 | **必须** |
| 说明 | 核心系统工具集。`wifiutil` 负责 insmod WiFi/BT 内核模块并初始化。如果将 WiFi/BT 驱动做成内核 built-in (`=y`) 而非 module (`=m`)，则 `wifiutil` 的模块加载功能不再需要，但其他工具（如 `simulate_key`）可能仍有用。如果驱动做成 module，则必须有此工具。 |

### aml-bt-ct

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/aml_bt_ct` |
| 产出 | `aml_bt_hciattach` + BT firmware (`fw_out.bin`, `fw_out.info`) |
| 依赖 | 无 |
| 必要性 | **必须** |
| 说明 | Amlogic BT HCI attach 工具。`w1_bt_start.sh` 脚本调用 `aml_hciattach` 来初始化 BT UART 并加载 firmware。没有它蓝牙无法初始化。这是用户态工具，需要做成 buildroot-external package。 |

### aml-commonlib

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/aml_commonlib` |
| 产出 | `aml_log` 库 + `aml_socketipc` 库 + 头文件 |
| 依赖 | 无 |
| 必要性 | **必须（如果使用 aml-util）** |
| 说明 | Amlogic 公共库，提供日志和 IPC 功能。aml-util 编译时依赖此库。如果不使用 aml-util（例如 WiFi/BT 驱动做成 built-in），则不需要。 |

### aml-ubootenv

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/aml_commonlib/ubootenv` |
| 产出 | `fw_printenv` / `fw_setenv` 工具 |
| 依赖 | zlib |
| 必要性 | **推荐** |
| 说明 | 从 Linux 用户空间读写 U-Boot 环境变量。对于调试、OTA 升级标记、启动模式切换很有用。buildroot 上游也有 `BR2_PACKAGE_UBOOT_TOOLS` 提供类似功能，但 Amlogic 的版本针对其特殊的 env 存储格式做了适配（eMMC 偏移地址等）。如果需要从 Linux 操作 U-Boot env，建议使用此 SDK 版本。 |

### aml-bootloader-message

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/aml_commonlib/bootloader_message` |
| 产出 | 读写 misc 分区 bootloader message 的工具 |
| 依赖 | zlib |
| 必要性 | **推荐** |
| 说明 | 用于读写 misc 分区中的 bootloader control block (BCB)。Android 风格的 recovery/OTA 机制依赖此工具来设置 `boot-recovery`、`boot-update` 等标记。如果 HubV3L 需要 A/B 分区切换或 recovery 模式，则需要此工具。当前阶段可以暂缓。 |

### aml-usb-config

| 项目 | 内容 |
|------|------|
| 源码 | `package/amlogic/aml-usb-config/src` |
| 产出 | `/etc/init.d/S89usbgadget` 启动脚本 |
| 依赖 | 无 |
| 必要性 | **不需要** |
| 说明 | USB Gadget 配置脚本（ADB + RNDIS 或 USB Mass Storage）。HubV3L 的 USB 口配置为 Host 模式，不使用 Gadget 功能。 |

---

## 音频相关

### aml-audio-player

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/aml_audio_player` |
| 产出 | 基于 FFmpeg 的音频播放器 |
| 依赖 | ffmpeg |
| 必要性 | **不需要** |
| 说明 | URL 音频播放器，用于智能音箱场景。HubV3L 是 IoT Hub，不做音频播放。 |

### aml-speaker-process

| 项目 | 内容 |
|------|------|
| 源码 | `package/amlogic/aml-speaker-process/src` |
| 产出 | 音箱音频处理工具 |
| 依赖 | avs-sdk (Amazon Voice Service) |
| 必要性 | **不需要** |
| 说明 | AVS 智能音箱的音频前处理（回声消除、唤醒词检测等）。HubV3L 不集成 AVS。 |

### aml-amaudioutils

| 项目 | 内容 |
|------|------|
| 源码 | `multimedia/aml_amaudioutils` |
| 产出 | 音频工具库 |
| 依赖 | boost, liblog, aml-commonlib |
| 必要性 | **不需要** |
| 说明 | Amlogic 音频 HAL 的工具库，用于音频路由、音量控制等。HubV3L 不使用 Amlogic 音频 HAL。 |

### dtv-audio-utils

| 项目 | 内容 |
|------|------|
| 源码 | `multimedia/aml_audio_hal` |
| 产出 | DTV 音频解码工具 |
| 依赖 | liblog, libbinder, aml-amaudioutils |
| 必要性 | **不需要** |
| 说明 | 数字电视音频解码工具。HubV3L 不涉及 DTV 功能。 |

### aml-dsp-util

| 项目 | 内容 |
|------|------|
| 源码 | `vendor/amlogic/rtos/dsp_util` |
| 产出 | HiFi DSP 通信工具 |
| 依赖 | alsa-lib (可选) |
| 必要性 | **不需要** |
| 说明 | 与 Amlogic HiFi4 DSP 通信的用户态工具。用于 DSP 固件加载和音频桥接。HubV3L 不使用 DSP 音频处理。 |

---

## 其他

### libtotem

| 项目 | 内容 |
|------|------|
| 源码 | 上游 totem-pl-parser 3.10.6 |
| 产出 | `libtotem-plparser.so` 播放列表解析库 |
| 依赖 | libglib2, libtool, glib-networking, libsoup, libxml2, libgmime, libarchive, libgcrypt |
| 必要性 | **不需要** |
| 说明 | GNOME 播放列表解析库，用于 VLC/GStreamer 等多媒体播放器。HubV3L 不做多媒体播放。 |

### aml-uboot-customer

| 项目 | 内容 |
|------|------|
| 源码 | 由 `BR2_PACKAGE_AML_UBOOT_CUSTOMER_GIT_REPO_URL` 配置 |
| 产出 | 客户定制 U-Boot 相关文件 |
| 依赖 | 无 |
| 必要性 | **不需要** |
| 说明 | 空壳 package，仅用于从 Git 拉取客户定制的 U-Boot 配置。HubV3L 已经通过 `uboot-legacy` package 处理 U-Boot。 |

---

## 总结

| 优先级 | Package | 集成方式 |
|--------|---------|---------|
| **必须** | aml-wifi | kernel patch（源码加入内核树） |
| **必须** | aml-bt | kernel patch（源码加入内核树，依赖 aml-wifi） |
| **必须** | aml-bt-ct | buildroot-external package |
| **必须（条件）** | aml-util | buildroot-external package（如果 WiFi/BT 做成 module） |
| **必须（条件）** | aml-commonlib | buildroot-external package（如果使用 aml-util） |
| **推荐** | aml-ubootenv | buildroot-external package |
| **推荐** | aml-bootloader-message | buildroot-external package |
| **不需要** | wifi-fw, dhd_priv, bt-setup, bluez5-utils(SDK版), web-ui-wifi | — |
| **不需要** | aml-audio-player, aml-speaker-process, aml-amaudioutils, dtv-audio-utils, aml-dsp-util | — |
| **不需要** | libtotem, aml-uboot-customer, aml-usb-config | — |
