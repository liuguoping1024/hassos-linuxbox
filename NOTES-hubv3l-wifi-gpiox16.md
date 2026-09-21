# HubV3L WiFi bring-up 记录 (2026-08-19)

## 原理图确认 (LinuxBox Lite WiFi/BT combo)
- WIFI_PWREN     = GPIOX_7  (C3)  -> aml_wifi power_on   [唯一上电脚]
- WIFI_WAKE_HOST = GPIOX_6  (A1)  -> aml_wifi interrupt
- WIFI_SD_D0..D3/CLK/CMD = GPIOX_0..5 (SDIO)
- Lite 的 WiFi 没有第二路上电 (无 power_on_2)
- GPIOX_16 = T_TXD2 (D10, coreUART_4-2) -> Thread(BL706) UART TX, 以后要用
- GPIOX_17 = T_RXD2 -> Thread UART RX

## 问题根因
dts 从 Sensi V3 抄了 aml_wifi 的 power_on_2 = GPIOX_16, 但 Lite 上 GPIOX_16 是 Thread 的,
WiFi 无第二路上电。驱动 assert 错误的 GPIOX_16 -> 上电时序乱 -> insmod vlsicomm 整机 hang。

## 本次改动 (先让 WiFi 跑)
1. 删除 aml_wifi 的 power_on_2-gpios (GPIOX_16) —— 核心修复
2. 暂时 disable Thread uart_B (占用 GPIOX_16) —— 专注 WiFi

## TODO (WiFi 通了以后)
- 恢复 Thread uart_B (GPIOX_16=T_TXD2 / GPIOX_17=T_RXD2), 此时不再与 WiFi 冲突

## 2026-08-19 SDIO 200MHz + /etc/wifi/w1 对齐 SDK
- 禁用补丁 1015（sd_emmc_b max-frequency 50MHz→200MHz），与 SDK axg_s420_v03.dts 完全一致（sdr104+200MHz，其余 init_core_phase=3/init_tx_phase=0/no-mmc/no-sd 本就相同）。
- aml-wifi.mk: rf 配置除 /lib/firmware/w1 外，增加安装到 /etc/wifi/w1（SDK 路径；multi_wifi_load_driver 用 conf_path=/etc/wifi/w1，之前该目录不存在→读不到 RF 配置，疑似 station 1 整机卡死原因之一）。
- PWM 32k 链条经确认已与 SDK 一致：aml_wifi.pwm_config→wifi_pwm_conf→pwm_aocd(MESON_PWM_0/2)，pinctrl wifi_32k_pwmao_c_pins=group pwm_ao_c_ao8；补丁1017 关掉 pwm_aoab（原误配 pwm_ao_c_pins1，抢 GPIOAO_8）。
- hubv3l 用 /lib/firmware/w1/w1_start.sh 加载：insmod aml_sdio → insmod vlsicomm(无参) → wlan0 up。SDK 用 multi_wifi_load_driver(带 con_mode=0x06 conf_path=... 等)。
- 镜像 sha256: ad68088be06d9c1867cf8c2a09878e7ef63ce07bc893b2561695c44b0b128a2f
- 待验证：station 1 是否还卡死。若仍卡→怀疑 200MHz 信号完整性或 vlsicomm 固件下载本身，需抓 insmod 前后 dmesg。

## 2026-08-19 (续) rf txt 对齐 SDK 5.4 + service 现状
- aml_wifi_rf*.txt: hubv3l 的 aml-wifibt-fw 原带 18 个(含 13 个 6.6 时代 chip-id txt),且 aml_wifi_rf_0321.txt 是 version=1(占位),与 SDK 5.4 驱动期望的 version=20211008 不符。已把 files/wifi/w1 精确替换为 SDK 5.4 驱动自带的 5 个(0321/ampak/fn_link/iton/rf.txt),删掉 13 个多余的。旧文件备份在 files/wifi/w1/.bak_6.6_rf_20260819/。
- 分工:/lib/firmware/w1 由 aml-wifibt-fw 独占(驱动默认路径);/etc/wifi/w1 由 aml-wifi 装(multi_wifi_load_driver 的 conf_path)。两处都是 SDK 5 个。
- 驱动默认 conf_path 已从 /vendor/etc/wifi/w1 改为 /lib/firmware/w1(wifi_hal_cmd.c + wifi_drv_config.c)。
- service 现状:amlogicw1.service(跑 w1_start.sh 加载 wifi)与 amlogicw1-bt.service 都在,但被 mask(hubv3l overlay 里 amlogicw1.service -> /dev/null)+ preset disable。注释"disabled until driver loading is stable"。待 wifi 手动验证稳定后再解 mask+enable。
- 最新镜像 sha256: 2bf4c129ca61c04b8be7b0e53d6bb9002276aca8ceacd0c1b8b638dbccf19db0

## 2026-08-19 ★真正根因: 补丁0994 bypass 了 mmc 时钟框架
- 用户确认: SDK image 和 Lite 是同一块 PCB(Sensi V3 多音频),SDK WiFi 能跑 → 不是硬件/信号问题,是 kernel 软件差异。
- 实测定位: echo 1 > /sys/class/aml_wifi/power 卡死,日志停在 usb_power_control "Set WiFi power on!" 之后,即 sdio_reinit()->sdio_rescan()->mmc_detect_change+flush_work 里 mmc 扫描 SDIO 卡挂死。meson-gx-mmc 传输是 wait_for_completion 无超时 → 任意 SDIO 传输不完成即无限挂死。
- HAOS kernel = 主线5.4.180 + AML补丁。meson-gx-mmc.c/sdio.c/core.c 与 SDK 基本一致(仅2行)。那2行来自补丁 0994-mmc-meson-gx-force-direct-clk-register-write.patch: 把 `if(host->run_pxp_flag==0)` 两处改成 `if(0)`,即跳过 (a)meson_mmc_clk_set 里的 clk_set_parent/clk_set_rate 时钟配置,(b)probe 里 devm_clk_get("core") 整段时钟获取。→ SDIO 时钟无法动态切到 SDR104/tuning → 扫描挂死。eMMC 因 u-boot 配好固定时钟仍能用。
- sd_emmc_b DT clocks/clock-names 与 SDK 完全一致; axg.c 时钟驱动提供 CLKID_SD_EMMC_B_* → 时钟框架本可用,0994 是错误的偷懒改法。
- 修复: 禁用 0994 (.disabled),恢复 SDK 一致的 meson-gx-mmc.c。已重编内核,run_pxp_flag==0 两处恢复,wifi/bt .ko 重装。
- 镜像 sha256: 67e7a1daf706588eb70830ac56f19f62ea116d5d7c4072e8f152635a63e965c3
- 风险: 0994 影响所有 mmc 实例(含 eMMC/rootfs)。已确认 DT+clk 驱动与 SDK 同,SDK 能启动 eMMC,故还原应安全; 若开不了机需 USB Burning Tool 重刷。
