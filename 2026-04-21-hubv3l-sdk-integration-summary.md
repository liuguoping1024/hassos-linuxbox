# 2026-04-21 HubV3L SDK 集成工作总结

## 目标

把 Amlogic A113X (AXG) SDK 的 u-boot / kernel / 打包流程集成到
HAOS (hassos-linuxbox) 的 buildroot 体系中，产出可通过 Amlogic USB
Burning Tool 烧录的 `aml_upgrade_package.img`，rootfs 使用 HAOS 的
erofs 替换 SDK 原生的 ubifs。

## 最终方案

单 buildroot + BR2_EXTERNAL + custom package（方案 C 修订版）：

- HAOS 的 buildroot 2024.02.11 保持不变，其他 board 不受影响
- 通过 BR2_EXTERNAL 引入 Amlogic 专属组件
- u-boot 做一个 custom package `uboot-legacy` 自己管编译和 FIP 打包
- kernel 用 buildroot 原生 `BR2_LINUX_KERNEL`，从 upstream 下载后
  打 SDK patch
- 打包链路从 SDK 搬来的预编译工具（mkbootimg, dtbTool,
  aml_image_v2_packer_new, aml_upgrade_pkg_gen.sh）完成

## 源码 / 二进制来源

| 组件 | 来源 | 处理方式 |
|------|------|----------|
| u-boot | upstream v2015.01 | buildroot 下载 + 51 个 SDK patch |
| kernel | upstream v5.4.180 | buildroot 下载 + 46 个 SDK patch |
| bl2 / bl30 / bl31 | SDK 预编译 | 搬入 `aml-fip/`，axg only |
| bl301 | 预编译 fallback | arm-none-eabi-gcc 缺失时用预编译 |
| fip_create / aml_encrypt_axg / acs_tool.pyc | SDK 工具 | 搬入 `aml-fip/fip/` |
| mkbootimg / dtbTool | SDK buildroot | 搬入 `aml-tools/` |
| aml_image_v2_packer_new / aml_upgrade_pkg_gen.sh / res_packer | SDK host | 搬入 `aml-tools/` |
| rootfs.cpio.gz (ramdisk) | SDK 预编译 | 搬入 `aml-fip/` |
| logo.img | SDK 预编译 | 搬入 `aml-fip/` |
| upgrade-axg 模板 / aml-user-key.sig | SDK | 搬入 `upgrade-axg/` 和 `aml-fip/binary/` |

## 目录结构

```
buildroot-external/
├── Config.in                                  # 加一行 source uboot-legacy/Config.in
├── linux/
│   └── linux-ext-aml-fixperms.mk             # kernel 脚本 +x 修复 hook
├── package/
│   └── uboot-legacy/
│       ├── Config.in
│       └── uboot-legacy.mk                    # 下载 + patch + build + FIP
├── configs/
│   └── thirdreality_hubv3l_defconfig
└── board/thirdreality/
    ├── kernel-linuxbox-hubv3l.config
    └── hubv3l/
        ├── hassos-hook.sh                     # post-image，dtb/boot/pack
        ├── meta
        ├── boot-env.txt, cmdline.txt
        ├── rootfs-overlay/                    # HAOS 层文件
        ├── scripts/
        │   └── build-fip.sh                   # FIP 打包驱动
        ├── patches/
        │   ├── linux/                         # 46 SDK + 4 本地 (0996~0999)
        │   └── uboot-legacy/                  # 51 SDK + 2 本地 (0950, 1001)
        ├── aml-fip/                           # 预编译 BL + 工具 (axg only)
        │   ├── bl2/bin/axg/bl2.bin
        │   ├── bl30/bin/axg/bl30.bin
        │   ├── bl301/bin/axg/bl301.bin
        │   ├── bl31_1.3/bin/axg/{bl31.bin,bl31.img}
        │   ├── fip/{fip_create,acs_tool.pyc,axg/aml_encrypt_axg,...}
        │   ├── binary/board/amlogic/axg_hubv3l_v1/aml-user-key.sig
        │   ├── rootfs.cpio.gz
        │   └── logo.img
        ├── aml-tools/                         # host 打包工具
        │   ├── aml_image_v2_packer_new
        │   ├── aml_upgrade_pkg_gen.sh
        │   ├── dtbTool
        │   ├── mkbootimg
        │   └── res_packer
        └── upgrade-axg/                       # 升级配置模板
            ├── aml_upgrade_package*.conf
            ├── aml_sdc_burn.ini
            ├── platform.conf
            ├── keys.conf
            └── aml-user-key.sig
```

## 构建流程

```
sudo ./make-buildroot-release.sh -b hubv3l [clean]
```

展开后：

1. **uboot-legacy package**
   - 下载 upstream u-boot v2015.01
   - 应用 51 个 SDK patch + `1001-Add-axg-hubv3l-v1-board.patch` +
     `0950-add-missing-empty-kconfig-files.patch`
   - `make axg_hubv3l_v1_config && make`
   - `|| true` 容错（bl301 编译可能因 arm-none-eabi-gcc 缺失失败，
     之后用预编译 fallback）
   - install-images 阶段调 `scripts/build-fip.sh`：
     - bl30 + bl301 → bl30_new.bin
     - acs_tool 注入 DDR 参数到 bl2
     - bl2 + bl21 → bl2_new.bin
     - fip_create 打 fip.bin
     - aml_encrypt_axg
       - `--bl3sig --compress lz4 --level v3 --type bl33`（BL33 LZ4 压缩签名）
       - `--bl2sig` / `--bootmk` / `--efsgen` / `--bootsig`
     - 产出 `u-boot.bin*` 全套放到 `$(BINARIES_DIR)`

2. **kernel**
   - buildroot 下载 upstream v5.4.180
   - linux-ext-aml-fixperms.mk 修复 scripts/*.sh 权限
   - 应用 46 SDK patch + 4 本地 patch：
     - `0996-dts-disable-nand-hubv3l-uses-emmc.patch`
     - `0997-dts-set-amlogic-dt-id-for-hubv3l.patch`
     - `0998-fix-dtb-build-remove-customer-dir.patch`
     - `0999-disable-werror-for-new-gcc.patch`
   - 编 `Image.gz` + `amlogic/axg_s420_1g.dtb`

3. **rootfs**
   - buildroot 生成 erofs

4. **post-image (hassos-hook.sh)**
   - `dtbTool` + gzip → `dtb.img`
   - `mkbootimg` 合并 `Image.gz` + `rootfs.cpio.gz`（SDK 预编译 ramdisk）
     + `dtb.img`，cmdline `root=/dev/system rootfstype=erofs ro rootwait
     init=/sbin/init console=ttyS0,115200n8` → `boot.img`
   - `cp boot.img → recovery.img`
   - `cp rootfs.erofs → rootfs.ubi`（system 分区原始内容）
     `cp rootfs.erofs → rootfs.ubifs`（骗 pkg_gen.sh 走 NAND 分支）
   - 复制 upgrade-axg 模板 + logo.img
   - `aml_upgrade_pkg_gen.sh axg "" ""`
     → `res_packer` 生成 logo（失败不影响）
     → `aml_image_v2_packer_new` 打最终 `aml_upgrade_package.img`

## 解决的关键问题（按时间顺序）

1. **busybox 动态链接 / initramfs 死路**：放弃自己手搓 initramfs，直接
   复用 SDK 预编译的 `rootfs.cpio.gz`。
2. **SDK 打包流程理解**：SDK 顶层 `Makefile` 就是 `include
   $(TARGET_OUTPUT_DIR)/Makefile`，SDK 本身就是一棵 buildroot 2020.02。
   所有"打包"逻辑分散在 `buildroot/fs/`, `buildroot/linux/`,
   `package/amlogic/aml_img_packer_new` 和 `board/amlogic/common/upgrade/`
   里，由 buildroot 调度。
3. **patch 以 SDK `/patches` 目录为准**：不从 SDK `kernel/` 和
   `bootloader/` 目录复制源码，用 upstream + patch 重建。
4. **kernel scripts 权限丢失**：tarball 解压后 `.sh` 和 `.pl` 没有 +x。
   通过 BR2_EXTERNAL 的 `linux-ext-*.mk` 机制加 `LINUX_POST_PATCH_HOOKS`
   修复。
5. **GCC 12 + kernel 5.4 = -Werror 死路**：`0999-disable-werror-for-new-gcc.patch`
   把 SDK 加的 `-Werror` 改成 `-Wno-error`。
6. **DTB 编译找 customer/ 目录**：SDK Makefile 里 `CONFIG_AMLOGIC_MODIFY`
   的 DTB 规则会去 `customer/arch/...`。`0998-fix-dtb-build-remove-customer-dir.patch`
   简化成直接 `$(dtstree)/$@`。
7. **u-boot 板子不匹配**：改用 `axg_hubv3l_v1` 替代 `axg_s420_v1`。
8. **drivers/vpu/Kconfig 缺失**：SDK patch 没带空文件。加
   `0950-add-missing-empty-kconfig-files.patch`。
9. **bl301 编译失败**：SDK 原本就这样，脚本加 `|| true` 容错，FIP 打包时
   从 `aml-fip/bl301/bin/axg/bl301.bin` 预编译 fallback。
10. **install-images 不跑**：buildroot 要求 `UBOOT_LEGACY_INSTALL_IMAGES = YES`。
11. **efsgen fail**：`aml-user-key.sig` 路径不对。复制到 hubv3l 板目录。
12. **u-boot.bin 大 530KB，BL30 crash**：对比 SDK 产物，发现缺
    `--compress lz4` 参数（`CONFIG_AML_BL33_COMPRESS_ENABLE=y`）。补上后
    u-boot.bin 大小跟 SDK 一致。
13. **USB Burning Tool aml_dt mismatch**：u-boot 设 `aml_dt=axg_jethubj100_v1`，
    但 DTB 的 `amlogic-dt-id=axg_s420_1g`。`0997-dts-set-amlogic-dt-id-for-hubv3l.patch`
    改 DTS 对齐。
14. **meson_nand probe 导致 kernel panic (list_add double add)**：NAND 驱动
    probe 失败后 `deferred_probe_work_func` 重试，parser 被重复注册。HubV3L
    用 eMMC，不需要 NAND。`0996-dts-disable-nand-hubv3l-uses-emmc.patch` 把
    DTS 里的 NAND 节点 disable。

## 验证结果

- **u-boot (BL33) 编译大小**：1,114,424 字节（与 SDK 一致）
- **FIP 产物 u-boot.bin 大小**：825,856 字节（加 BL33 LZ4 压缩后与 SDK 一致）
- **烧录**：`aml_upgrade_package.img` 通过 USB Burning Tool 烧录成功
- **启动链**：BL2 → BL30 → BL31 → BL33 (u-boot) → kernel 全部正常
- **kernel**：Linux 5.4.180 启动到 `smp: Brought up 1 node, 4 CPUs`，
  driver probe 正常进行（NAND disable 后不再 panic）
- **DDR**：单片 512MB 不稳（硬件问题，需换双片），双片 2x512MB 正常

## 仓库管理

- commit `9398f7185 feat(hubv3l): add Amlogic A113X (AXG) board support with SDK integration`
- 分支 `hubv3l-sdk54` 只推到 private remote
  `git@github.com:liuguoping1024/hassos-linuxbox-private.git`
- public remote `origin` (`liuguoping1024/hassos-linuxbox`) **未推送**
  （Amlogic 专有二进制和 patch 不适合公开）
- 187 个文件，67.67 MiB

## 清理的工作

- 删除 `tmp-kernel-patch/`（288M 旧方案 tarball）
- 删除 `aml-fip/` 里非 axg 平台 binary（a1/a5/c1/c2/c3/g12a/g12b/gxb/
  gxl/gxtvbb/p1/s4/s5/sc2/t3/t5/t5d/t5w/t7/tl1/tm2/txhd/txl/txlx）
  从 152M 瘦到 5M
- `.gitignore` 挡住 `*.log`

## 待办 / 已知问题

1. **DDR 单片配置**：硬件层问题，需要 bl2 DDR 参数调整或固定使用双片设备。
2. **ramdisk 自编译**：目前用 SDK 预编译 `rootfs.cpio.gz`。后续可改为
   buildroot 从 target 按 `ramfslist` 自己打包，消除对 SDK ramdisk 的依赖。
3. **recovery.img 独立**：目前是 boot.img 的拷贝，后续可做 HAOS 专属
   recovery ramdisk。
4. **HAOS userspace 启动**：kernel 起来了但 rootfs userspace 启动流程（
   systemd, supervisor, Docker, HA）尚未测试。
5. **`BR2_TARGET_UBOOT_AMLOGIC_BOOTARGS` cmdline 来源**：目前由 hook 里
   mkbootimg 的 `--cmdline` 参数决定，不依赖 u-boot。
