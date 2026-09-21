# Meson64 内核补丁（ThirdReality hub-v3 / v3a / v3b）

目标内核：Linux 6.6.120，arch/arm64，SoC 为 Amlogic A113D（AXG 家族）。

这些补丁通过 `BR2_GLOBAL_PATCH_DIR` 应用，buildroot 只会匹配 `*.patch`
（`package/pkg-generic.mk` 传 `\*.patch` 给 `support/scripts/apply-patches.sh`，
`scan_patchdir` 用 `ls -d` 展开）。把后缀改成 `.patch.disabled` 即可临时停用。

注意 `BR2_GLOBAL_PATCH_DIR` 含**两个**目录，另一个是
`buildroot-external/patches/linux/`（上游 HAOS 自己的 `0001-ipv6-...`），
它会先于本目录被应用，和这里的编号无关。

## 当前 17 个

| 补丁 | 作用 | 实机可见的证据 |
|---|---|---|
| 0010 | 砍掉上游 `early_init_dt_reserve_memory()` 里的 `-EBUSY` 检查 | `secmon@5000000 (3072 KiB) nomap non-reusable`，且无 `failed to reserve` |
| 0012 | meson-gx-mmc 支持 core/tx/rx 时钟相位 | `mmc0: new HS200`、`mmc1: new ultra high speed SDR50` |
| 0013 | meson-axg.dtsi 填入相位属性 | 同上 |
| 0014 | 0012/0013 的 DT binding 文档（纯 `Documentation/`，零代码） | — |
| 0021 | socinfo 加 S905L ID | 对本板无用，但与 0022/0023 改同一文件，保留以免上下文漂移 |
| 0022 | socinfo 加 A113X SoC ID | `soc soc0: Amlogic Meson AXG (A113X) Revision 25:b` |
| 0023 | socinfo 公共代码移入头文件（0024 依赖） | — |
| 0024 | 经 secure monitor 读 SoC 信息 | `meson-gx-socinfo-sm ...: got sm version call 2` |
| 0025 | meson-axg.dtsi 连上 ao-secure | `soc soc1: ... Detected (SM)` |
| 0026 | ao-secure 的 binding 文档 | — |
| 0035 | **GPIO 中断支持。所有 GPIO 中断都依赖它** | `irq_meson_gpio: 100 to 8 gpio interrupt mux initialized`，`gpio irq setup: ... pin[6] / pin[53] / pin[26]` |
| 0040 | Amlogic 精简驱动集（wifi_dt、bt_device、pwm-meson 等） | `wifi_dt_init` / `wifi_dev_probe` 全流程 |
| 0041 | Amlogic W155S1 WiFi/BT 驱动，220 个文件 | `aml_w1_sdio_probe`、`hci0 UP RUNNING` |
| 0042 | hub-v3 / hub-v3b DTS | `Machine model: ThirdReality Linuxbox Hub-v3` |
| 0043 | hub-v3a DTS | — |
| 0044 | 标注 `sdio_reinit()` 的调用来源（诊断） | `aml_wifi: sdio_reinit() from set_wifi_power(1)` |
| 0045 | aml_wifi 延后到 SDIO host 就绪后再 probe；IRQ 先 ack 再判空 | 正常启动无 `sdio_host is NULL`、无 `usb_power_control` |

`0040` 相对 armbian 的 `jethome-0003` 多带了 `drivers/pwm/core.c` 和
`include/linux/pwm.h` 两个文件，用来把 6.6 之前的 PWM API（`devm_of_pwm_get()`、
返回 int 的 `pwmchip_remove()`）backport 回来。armbian 那边已改用 6.6 原生的
`devm_fwnode_pwm_get()` 从而不再需要这两处内核核心手术 —— 是一个可以跟进的简化。

## 已删除的 18 个（原先从 armbian meson64 补丁集整目录抄入）

armbian 那套要覆盖所有 Amlogic 芯片，包括带 Mali GPU、HDMI、硬件视频解码、
红外遥控的电视盒子。A113D 是无头音频 SoC，上述硬件一概没有。删除依据是
反编译三个实际出货的 DTB 逐个查过 compatible，以及核对 kernel config：

| 补丁 | 删除依据 |
|---|---|
| 0001 | `meson64,reboot` 在三个 DTB 里出现 0 次，驱动永不 bind |
| 0003 | `amlogic,meson-gx-pm` 在三个 DTB 里出现 0 次 |
| 0004 | 重复定义上游已有的 `$(obj)/%.dtbo` 规则（上游用 `.dtso`，它用 `.dts`）；其 `DTC_FLAGS +=` 被 buildroot 命令行的 `DTC_FLAGS=-@` 覆盖；且本项目无任何 `.dtso`/`.scr-cmd` 源文件 |
| 0005 | panfrost = Mali GPU 驱动，A113D 无 GPU |
| 0007 | 2560x1440 支持，改 `drm/meson/meson_vclk.c`；DTB 里无 VPU / HDMI |
| 0011 | AIU 是 GX 系音频复合体，AXG 用另一套；DTB 里无 aiu 节点 |
| 0015 0016 0017 0032 0033 | HEVC / VP9 硬解码，共 59 KB。`CONFIG_VIDEO_MESON_VDEC` 未启用，一行未编入 |
| 0019 | `extra-y += $(dtbo-y) $(scr-y) $(dtbotxt-y)`，三个列表全空 |
| 0020 | DTB 里无 `meson-ir` 节点 |
| 0028 | XTX SPI NOR，DTB 里无 spi-nor 节点（本板为 eMMC） |
| 0029 | 仅把一条错误信息降为 info；触发条件 `bNbrPorts == 0` 从不成立（实机为 `hub 1-0:1.0: 1 port detected`） |
| 0030 | 改 `pinctrl-meson-g12a.c` 与 `meson-g12-common.dtsi`，与 AXG 无关 |
| 0031 | 改 `meson-gx.dtsi`，GX 专属 |
| 0034 | SM1 超频，`meson-sm1.dtsi`；SM1 的 DTB 不编译 |

注意：删除 0005 / 0011 / 0020 **不会**移除 `panfrost.ko`、
`snd-soc-meson-aiu.ko`、`meson-ir.ko` —— 这些模块由 kernel config
（`DRM_PANFROST=m`、`SND_MESON_AIU=m`、`IR_MESON=m`）决定，删掉的只是
armbian 针对它们的修复。由于对应硬件不存在，驱动永不 probe，那些修复本来也走不到。
**给不存在的硬件编译驱动是 config 层面的另一个问题，尚未处理。**

## 效果

删除后镜像体积几乎不变（burn 镜像 325,557,984 → 325,533,408 字节），
因为占比最大的 59 KB 视频解码代码本来就没有被编译。
真正的收益是维护量：升级内核时需要重新适配的补丁从 35 个降到 17 个，
其中 15 个是几 KB 的小补丁，主要工作量集中在 0040 / 0041 那 5.3 MB 的驱动包。

## 验证方式

在原始内核树上整条 series 走一遍：

```sh
tar -xf /cache/dl/linux/linux-6.6.120.tar.xz && cd linux-6.6.120
for f in <此目录>/*.patch; do
    patch -g0 -p1 -E --no-backup-if-mismatch -f < "$f" || echo "FAIL $f"
done
```

17 个应全部干净应用，offset/fuzz 计数与 35 个时一致
（0040 有 4 处 offset，0010 / 0025 各 1 处，其余为 0）。
