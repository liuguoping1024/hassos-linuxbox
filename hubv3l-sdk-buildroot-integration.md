# HubV3L SDK Buildroot 集成方案 (方案 C - 修订版)

## 背景

- Amlogic A113X SDK 本身就是一棵定制的 buildroot (2020.02)
- HAOS (hassos-linuxbox) 也是 buildroot (2024.02.11)
- 目标: 在 HAOS 的 buildroot 体系内, 通过 BR2_EXTERNAL 和 custom package,
  完整集成 Amlogic A113X 的 u-boot / kernel / 打包流程

## 核心原则

1. u-boot 必须重编 (hubv3l 板级定制, 基于 upstream v2015.01 + SDK patch)
2. kernel 必须重编 (hubv3l 硬件适配, 基于 upstream v5.4.180 + SDK patch)
3. rootfs 用 HAOS 的 erofs (替换 SDK 的 ubifs)
4. 不复制 SDK 的 kernel/bootloader 源码目录, 使用 patches/ 目录的 patch
5. FIP 预编译二进制 (bl2/bl30/bl31) 和工具 (fip_create/aml_encrypt_axg) 从 SDK 搬入
6. 继续使用 HAOS 的 buildroot 2024.02 (不换成 SDK 的 2020.02), 其他 board 不受影响

## SDK 构建流程分析

### SDK 目录结构 (与本方案相关的部分)

```
/disk4T/liuguoping/sdk_A113X_202210/
├── setenv.sh                    # source setenv.sh axg_s420_a6432_k54_release && make
├── Makefile                     # include $(TARGET_OUTPUT_DIR)/Makefile (就是 buildroot)
├── buildroot/                   # 定制版 buildroot 2020.02
│   ├── configs/axg_s420_a6432_k54_release_defconfig
│   ├── configs/amlogic/         # config fragments
│   ├── package/amlogic/         # ~120 个 Amlogic 专属 package
│   ├── boot/uboot/2019/         # Amlogic 定制 uboot.mk
│   ├── linux/mkbootimg/         # Android-style boot.img 打包
│   ├── linux/dtbTool/           # 多 DTB 合并
│   └── board/amlogic/common/    # initramfs, upgrade 模板
├── kernel/aml-5.4/              # Amlogic kernel 5.4 (不复制, 用 patch 重建)
├── bootloader/uboot-repo/       # Amlogic u-boot (不复制, 用 patch 重建)
│   ├── bl2/, bl30/, bl31_1.3/   # 预编译 BL 二进制 (需要搬入)
│   └── fip/                     # FIP 工具 (需要搬入)
└── patches/                     # ★ 你准备的 patch 和文档
    ├── uboot-v2015.01/          # 51 个 patch (基于 upstream v2015.01)
    ├── kernel-v5.4.180/         # 46 个 patch (基于 upstream v5.4.180)
    ├── tools/build_uboot.sh     # 独立编译脚本
    └── doc/README.md            # 详细文档
```

### SDK 构建命令

```bash
cd /disk4T/liuguoping/sdk_A113X_202210
unset LD_LIBRARY_PATH
source setenv.sh axg_s420_a6432_k54_release
make
```

`setenv.sh` 做了什么:
1. 扫描 `buildroot/configs/` 找到 `axg_s420_a6432_k54_release_defconfig`
2. 设置 `TARGET_OUTPUT_DIR=output/axg_s420_a6432_k54_release`
3. 执行 `make O=$TARGET_OUTPUT_DIR axg_s420_a6432_k54_release_defconfig`
4. 之后 `make` 就是标准 buildroot 构建

### SDK defconfig 关键配置

```ini
# 来自 axg_s420_a6432_k54_release_defconfig + fragment includes:

# Kernel: 本地源码, Amlogic 5.4
BR2_LINUX_KERNEL_CUSTOM_LOCAL=y
BR2_LINUX_KERNEL_CUSTOM_LOCAL_PATH="$(TOPDIR)/../kernel/aml-5.4"
BR2_LINUX_KERNEL_DEFCONFIG="meson64_a64_smarthome"
BR2_LINUX_KERNEL_IMAGE_LOADADDR="0x1008000"
BR2_LINUX_KERNEL_INTREE_DTS_NAME="axg_s420 axg_s420_v03 axg_s420_1g"
BR2_LINUX_KERNEL_ANDROID_FORMAT=y          # 生成 boot.img (Android header)

# U-Boot: 本地源码, Amlogic repo
BR2_TARGET_UBOOT=y
BR2_TARGET_UBOOT_CUSTOM_LOCAL=y
BR2_TARGET_UBOOT_CUSTOM_LOCAL_LOCATION="$(TOPDIR)/../bootloader/uboot-repo"
BR2_TARGET_UBOOT_AMLOGIC_REPO=y           # SDK 定制: 自动处理 fip 签名
BR2_TARGET_UBOOT_PLATFORM="axg"
BR2_TARGET_UBOOT_BOARDNAME="axg_s420_v1"

# Ramdisk: 从文件列表生成 (不是完整 rootfs)
BR2_TARGET_ROOTFS_INITRAMFS_LIST="board/amlogic/common/initramfs/initramfs-54/ramfslist-32-ubi-release"

# Rootfs: ubifs + ubi (NAND)
BR2_TARGET_ROOTFS_UBIFS=y
BR2_TARGET_ROOTFS_UBI=y

# Rootfs overlay
BR2_ROOTFS_OVERLAY="board/amlogic/mesonaxg_s420/rootfs/"

# Host 打包工具
BR2_PACKAGE_HOST_AML_IMG_PACKER_NEW=y
```

### SDK 构建产物链

```
buildroot make
  ├─ u-boot (fip: bl2 + bl31 + bl33) → u-boot.bin, u-boot.bin.usb.bl2/tpl, u-boot.bin.sd.bin
  ├─ kernel (Image.gz + DTBs)
  │   ├─ dtbTool → dtb.img (多 DTB 合并 + gzip)
  │   ├─ ramfslist → rootfs.cpio → rootfs.cpio.gz (最小 ramdisk)
  │   └─ mkbootimg → boot.img (kernel + ramdisk + dtb, Android header)
  │                 → recovery.img (kernel + recovery ramdisk + dtb)
  ├─ rootfs → rootfs.ubifs → rootfs.ubi (NAND 格式)
  └─ post-image:
      ├─ cp upgrade-axg/* → images/
      └─ aml_upgrade_pkg_gen.sh axg
           └─ aml_image_v2_packer_new → aml_upgrade_package.img (USB Burning Tool 烧录)
```

## 方案 C (修订): 单 buildroot + BR2_EXTERNAL + custom package

不换 buildroot 树。继续用 HAOS 的 buildroot 2024.02, 通过 BR2_EXTERNAL 引入所有 Amlogic 特有组件。

### 架构

```
hassos-linuxbox/
├── buildroot/                   # HAOS buildroot 2024.02 (不动)
├── buildroot-external/          # HAOS BR2_EXTERNAL
│   ├── configs/
│   │   └── thirdreality_hubv3l_defconfig
│   ├── package/
│   │   ├── uboot-legacy/       # [新增] custom package: 编译 u-boot + FIP
│   │   │   ├── uboot-legacy.mk
│   │   │   └── Config.in
│   │   └── hubv3l-aml-imgpack/ # [新增] host package: aml_image_v2_packer_new
│   │       ├── hubv3l-aml-imgpack.mk
│   │       └── Config.in
│   ├── board/thirdreality/hubv3l/
│   │   ├── hassos-hook.sh       # post-image: mkbootimg + rootfs替换 + aml打包
│   │   ├── rootfs-overlay/
│   │   ├── patches/
│   │   │   ├── uboot/           # 51 个 SDK u-boot patch (从 patches/uboot-v2015.01/)
│   │   │   └── linux/           # 46 个 SDK kernel patch (从 patches/kernel-v5.4.180/)
│   │   ├── aml-fip/             # [新增] 预编译二进制 + FIP 工具
│   │   │   ├── bl2/bin/axg/bl2.bin
│   │   │   ├── bl30/bin/axg/bl30.bin
│   │   │   ├── bl31_1.3/bin/axg/bl31.img
│   │   │   ├── fip/fip_create
│   │   │   ├── fip/acs_tool.pyc
│   │   │   ├── fip/axg/aml_encrypt_axg
│   │   │   └── binary/board/amlogic/axg_s420_v1/aml-user-key.sig
│   │   ├── aml-tools/           # [新增] mkbootimg, dtbTool, aml_image_v2_packer_new 等
│   │   ├── upgrade-axg/         # [新增] aml_upgrade_package.conf 等模板
│   │   └── logo.img             # [新增] 预编译 logo (直接从 SDK 拿)
│   └── ...
├── Makefile                     # 不改 (hubv3l 也走同一棵 buildroot)
└── make-buildroot-release.sh    # 不改 (hubv3l 也走 BR2_EXTERNAL 流程)
```

### 源码来源

| 组件 | 来源 | patch |
|------|------|-------|
| u-boot | upstream v2015.01 tarball | patches/uboot-v2015.01/ (51个) |
| kernel | upstream v5.4.180 tarball | patches/kernel-v5.4.180/ (46个) |
| bl2/bl30/bl31 | SDK 预编译 (搬入 aml-fip/) | 无 (二进制) |
| fip_create, aml_encrypt_axg, acs_tool.pyc | SDK 工具 (搬入 aml-fip/) | 无 (二进制) |
| mkbootimg, dtbTool | SDK buildroot/linux/ (搬入 aml-tools/) | 无 |
| aml_image_v2_packer_new | SDK host package (搬入 aml-tools/) | 无 |

### u-boot 编译策略

buildroot 原生的 `BR2_TARGET_UBOOT` 不适用, 因为:
- Amlogic u-boot 2015.01 用 `make board_config` 而非 `make defconfig`
- FIP 链路 (bl2+bl30+bl31+bl33 → fip_create → aml_encrypt_axg) 完全外挂

所以做一个 **custom package `uboot-legacy`**:
- 下载 upstream u-boot v2015.01 tarball
- 应用 51 个 SDK patch
- 运行 `make axg_s420_v1_config && make`
- 执行 FIP 打包 (复用 build_uboot.sh 的逻辑)
- 把 u-boot.bin* 安装到 $(BINARIES_DIR)

### kernel 编译策略

buildroot 原生的 `BR2_LINUX_KERNEL` 可以直接用:
```ini
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_VERSION=y
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="5.4.180"
BR2_LINUX_KERNEL_PATCH="$(BR2_EXTERNAL_HASSOS_PATH)/board/thirdreality/hubv3l/patches/linux"
BR2_LINUX_KERNEL_DEFCONFIG="meson64_a64_smarthome"
BR2_LINUX_KERNEL_IMAGE_TARGET="Image.gz"
BR2_LINUX_KERNEL_DTS_SUPPORT=y
BR2_LINUX_KERNEL_INTREE_DTS_NAME="axg_s420_1g"
```

buildroot 会自动: 下载 v5.4.180 → 应用 46 个 patch → 编译 Image.gz + DTB

注意: 必须屏蔽 HAOS 原有的 kernel patch。HAOS buildroot-external 可能有
`BR2_LINUX_KERNEL_PATCH` 或 `BR2_GLOBAL_PATCH_DIR` 指向其他 board 的 patch,
hubv3l 的 defconfig 里要明确覆盖这些, 只指向 SDK 的 46 个 patch。
同理, HAOS 原有的 kernel defconfig (如果有全局的) 也要被 hubv3l 的覆盖。

### boot.img 生成策略

在 post-image hook (hassos-hook.sh) 里用 SDK 的 mkbootimg:
```bash
mkbootimg \
    --kernel  ${BINARIES_DIR}/Image.gz \
    --ramdisk ${BINARIES_DIR}/rootfs.cpio.gz \
    --second  ${BINARIES_DIR}/dtb.img \
    --base 0x0 --kernel_offset 0x1080000 \
    --cmdline "root=/dev/system rootfstype=erofs ro rootwait init=/sbin/init console=ttyS0,115200" \
    --output  ${BINARIES_DIR}/boot.img
```

cmdline 直接写成 erofs, 不需要改 SDK buildroot 的任何东西。

### ramdisk (rootfs.cpio.gz) 策略

SDK 的 ramdisk 是 buildroot 从 target 里按 `ramfslist-32-ubi-release` 文件列表 cpio 出来的
(342 个文件, 包含 busybox + libc + init + ubi 工具等)。

理论上可以自己编, 但先用 SDK 预编译的 rootfs.cpio.gz, 等其他部分跑通再回来做。

### boot.img 生成

SDK 的 `linux.mk` 在 kernel 编译完后调用 `linux/mkbootimg`:
```bash
mkbootimg --kernel Image.gz \
    --ramdisk rootfs.cpio.gz \
    --second dtb.img \
    --base 0x0 --kernel_offset 0x1080000 \
    --cmdline "root=/dev/ubi0_0 rootfstype=ubifs init=/sbin/init ..." \
    --output boot.img
```
我们在 post-image hook 里用同样的 mkbootimg, 只改 cmdline 为 erofs。

### dtb.img 生成

SDK 的 `linux.mk` 在 kernel 编译完后调用 `linux/dtbTool`:
```bash
dtbTool -o dtb.img -p <kernel-build>/scripts/dtc/ <images-dir>/
```
扫描 images 目录里所有 `.dtb` 文件, 合并成多 DTB 镜像, 再 gzip。
我们在 post-image hook 里同样做。

### logo.img

SDK 用 `res_packer` 从 `logo_img_files/` 目录打包。
从 build log 看 SDK 自己也找不到那个目录 (`opendir failed`),
但 `logo.img` 已经预生成在 SDK images 里。
直接拿来用, 作为预编译 blob 放进 `aml-fip/logo.img`。

### 最终打包

post-image hook:
1. dtbTool → dtb.img (多 DTB 合并)
2. mkbootimg → boot.img (kernel + ramdisk + dtb)
3. cp boot.img → recovery.img
4. cp rootfs.erofs → rootfs.ubi (system 分区 payload)
5. aml_upgrade_pkg_gen.sh axg → aml_upgrade_package.img

### post-image hook (hassos-hook.sh) 简化版

方案 C 下, SDK buildroot 已经帮我们生成了:
- u-boot.bin* (全套)
- dtb.img
- boot.img (kernel + ramdisk + dtb)
- recovery.img

hook 只需要做:
1. 把 HAOS 的 rootfs.erofs 复制为 rootfs.ubi (system 分区 payload)
2. 保留 rootfs.ubifs 让 aml_upgrade_pkg_gen.sh 走 NAND 分支
3. 调用 aml_upgrade_pkg_gen.sh 打最终包

```bash
function hassos_post_image() {
    # rootfs.erofs → rootfs.ubi (system 分区, 原始 blob)
    cp -f "${BINARIES_DIR}/rootfs.erofs" "${BINARIES_DIR}/rootfs.ubi"
    cp -f "${BINARIES_DIR}/rootfs.erofs" "${BINARIES_DIR}/rootfs.ubifs"

    # 调用 SDK 原生打包
    BINARIES_DIR="${BINARIES_DIR}" TOOL_DIR="${HOST_DIR}/usr/bin" \
        "${HOST_DIR}/usr/bin/aml_upgrade_pkg_gen.sh" "axg" "" ""
}
```

注意: 因为用的是 SDK buildroot, `HOST_DIR` 里已经有 `aml_image_v2_packer_new`、
`aml_upgrade_pkg_gen.sh`、`res_packer` 等工具, 不需要从外部 SDK 路径引用。

## 实施步骤

### Phase 1: 搬入 SDK 二进制和工具

```bash
# 1. FIP 预编译二进制 + 工具
mkdir -p buildroot-external/board/thirdreality/hubv3l/aml-fip
cp -a /disk4T/.../bootloader/uboot-repo/bl2    aml-fip/
cp -a /disk4T/.../bootloader/uboot-repo/bl30   aml-fip/
cp -a /disk4T/.../bootloader/uboot-repo/bl31_1.3  aml-fip/
cp -a /disk4T/.../bootloader/uboot-repo/fip    aml-fip/
cp -a /disk4T/.../patches/uboot-v2015.01/binary  aml-fip/

# 2. 打包工具
mkdir -p buildroot-external/board/thirdreality/hubv3l/aml-tools
cp /disk4T/.../buildroot/linux/mkbootimg/*      aml-tools/
cp /disk4T/.../buildroot/linux/dtbTool/*        aml-tools/
cp /disk4T/.../output/.../host/usr/bin/aml_image_v2_packer_new  aml-tools/
cp /disk4T/.../output/.../host/usr/bin/aml_upgrade_pkg_gen.sh   aml-tools/
cp /disk4T/.../output/.../host/usr/bin/res_packer               aml-tools/

# 3. 升级模板
cp -a /disk4T/.../buildroot/board/amlogic/common/upgrade/upgrade-axg  \
    buildroot-external/board/thirdreality/hubv3l/upgrade-axg/

# 4. SDK ramdisk (预编译 blob, 后续可自己编)
cp /disk4T/.../output/.../images/rootfs.cpio.gz  \
    buildroot-external/board/thirdreality/hubv3l/aml-fip/

# 4b. logo.img (预编译, 直接使用)
cp /disk4T/.../output/.../images/logo.img  \
    buildroot-external/board/thirdreality/hubv3l/aml-fip/

# 5. u-boot patch
cp /disk4T/.../patches/uboot-v2015.01/*.patch  \
    buildroot-external/board/thirdreality/hubv3l/patches/uboot/

# 6. kernel patch
cp /disk4T/.../patches/kernel-v5.4.180/*.patch  \
    buildroot-external/board/thirdreality/hubv3l/patches/linux/
```

### Phase 2: 创建 uboot-legacy custom package

`buildroot-external/package/uboot-legacy/Config.in`:
```
config BR2_PACKAGE_UBOOT_LEGACY
    bool "uboot-legacy"
    help
      Amlogic A113X u-boot for HubV3L.
      Downloads upstream v2015.01, applies SDK patches, builds with FIP.
```

`buildroot-external/package/uboot-legacy/uboot-legacy.mk`:
```makefile
UBOOT_LEGACY_VERSION = v2015.01
UBOOT_LEGACY_SITE = https://github.com/u-boot/u-boot.git
UBOOT_LEGACY_SITE_METHOD = git
UBOOT_LEGACY_BOARD = axg_s420_v1
UBOOT_LEGACY_FIP_DIR = $(BR2_EXTERNAL_HASSOS_PATH)/board/thirdreality/hubv3l/aml-fip
UBOOT_LEGACY_PATCH_DIR = $(BR2_EXTERNAL_HASSOS_PATH)/board/thirdreality/hubv3l/patches/uboot

define UBOOT_LEGACY_CONFIGURE_CMDS
    cd $(@D) && $(MAKE) $(UBOOT_LEGACY_BOARD)_config
endef

define UBOOT_LEGACY_BUILD_CMDS
    # 编译 BL33
    cd $(@D) && $(MAKE) -j$(PARALLEL_JOBS) \
        CROSS_COMPILE=aarch64-elf- \
        SYSTEMMODE=null AVBMODE=null BOOTCTRLMODE=null FASTBOOTMODE=null AVB2RECOVERY=null
    # FIP 打包
    $(BR2_EXTERNAL_HASSOS_PATH)/board/thirdreality/hubv3l/scripts/build-fip.sh \
        $(@D) $(UBOOT_LEGACY_FIP_DIR) $(UBOOT_LEGACY_BOARD) $(BINARIES_DIR)
endef

define UBOOT_LEGACY_INSTALL_IMAGES_CMDS
    # u-boot.bin* 已经被 build-fip.sh 放到 BINARIES_DIR
endef

$(eval $(generic-package))
```

### Phase 3: 更新 defconfig

`buildroot-external/configs/thirdreality_hubv3l_defconfig`:
```ini
# ---- 架构 ----
BR2_aarch64=y
BR2_cortex_a53=y

# ---- 工具链 ----
# (使用 buildroot 内置或外部工具链, 视情况而定)

# ---- Kernel ----
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_VERSION=y
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="5.4.180"
BR2_LINUX_KERNEL_PATCH="$(BR2_EXTERNAL_HASSOS_PATH)/board/thirdreality/hubv3l/patches/linux"
BR2_LINUX_KERNEL_USE_DEFCONFIG=y
BR2_LINUX_KERNEL_DEFCONFIG="meson64_a64_smarthome"
BR2_LINUX_KERNEL_IMAGE_TARGET="Image.gz"
BR2_LINUX_KERNEL_DTS_SUPPORT=y
BR2_LINUX_KERNEL_INTREE_DTS_NAME="axg_s420_1g"

# ---- U-Boot (custom package, 不用 BR2_TARGET_UBOOT) ----
# BR2_TARGET_UBOOT is not set
BR2_PACKAGE_UBOOT_LEGACY=y

# ---- Rootfs ----
BR2_TARGET_ROOTFS_EROFS=y
# BR2_TARGET_ROOTFS_UBIFS is not set

# ---- HAOS packages ----
# (hassio, supervisor, docker, systemd 等)

# ---- Post-image ----
BR2_ROOTFS_POST_IMAGE_SCRIPT="$(BR2_EXTERNAL_HASSOS_PATH)/board/thirdreality/hubv3l/hassos-hook.sh"
```

### Phase 4: 简化 hassos-hook.sh

```bash
function hassos_post_image() {
    local TOOLS="${BR2_EXTERNAL_HASSOS_PATH}/board/thirdreality/hubv3l/aml-tools"
    local UPGRADE="${BR2_EXTERNAL_HASSOS_PATH}/board/thirdreality/hubv3l/upgrade-axg"

    # 1. dtbTool → dtb.img
    "${TOOLS}/dtbTool" -o "${BINARIES_DIR}/dtb.img" \
        -p "${BUILD_DIR}/linux-5.4.180/scripts/dtc/" "${BINARIES_DIR}/"
    gzip -f "${BINARIES_DIR}/dtb.img"
    mv "${BINARIES_DIR}/dtb.img.gz" "${BINARIES_DIR}/dtb.img"

    # 2. mkbootimg → boot.img
    "${TOOLS}/mkbootimg" \
        --kernel "${BINARIES_DIR}/Image.gz" \
        --ramdisk "${BR2_EXTERNAL_HASSOS_PATH}/board/thirdreality/hubv3l/aml-fip/rootfs.cpio.gz" \
        --second "${BINARIES_DIR}/dtb.img" \
        --base 0x0 --kernel_offset 0x1080000 \
        --cmdline "root=/dev/system rootfstype=erofs ro rootwait init=/sbin/init console=ttyS0,115200" \
        --output "${BINARIES_DIR}/boot.img"

    # 3. recovery.img = boot.img
    cp -f "${BINARIES_DIR}/boot.img" "${BINARIES_DIR}/recovery.img"

    # 4. rootfs.erofs → rootfs.ubi (system 分区)
    cp -f "${BINARIES_DIR}/rootfs.erofs" "${BINARIES_DIR}/rootfs.ubi"
    cp -f "${BINARIES_DIR}/rootfs.erofs" "${BINARIES_DIR}/rootfs.ubifs"

    # 5. 复制升级模板
    cp -f "${UPGRADE}"/* "${BINARIES_DIR}/"

    # 6. 打包 aml_upgrade_package.img
    BINARIES_DIR="${BINARIES_DIR}" TOOL_DIR="${TOOLS}" \
        "${TOOLS}/aml_upgrade_pkg_gen.sh" "axg" "" ""
}
```

## 风险点

1. **buildroot 2024.02 vs kernel 5.4.180 兼容性**
   - 新版 buildroot 的 linux.mk 可能对老 kernel 有不兼容改动
   - 如有问题, 通过 kernel patch 或 linux.mk override 解决

2. **u-boot 工具链**
   - Amlogic u-boot 2015.01 需要 aarch64-elf-gcc (Linaro 7.5, bare-metal)
   - 这跟 buildroot 的 linux 工具链 (aarch64-none-linux-gnu) 不同
   - hubv3l-uboot package 需要自己管理工具链路径

3. **FIP 工具是 x86_64 二进制**
   - fip_create, aml_encrypt_axg 是 host 端 x86_64 ELF
   - 只能在 x86_64 构建机上跑 (Docker 里也行)

4. **erofs 在 NAND 上的可行性**
   - erofs 是块设备 fs, NAND 上需要通过 ubiblock 暴露
   - Amlogic kernel 的分区驱动把 system 暴露为 /dev/system (块设备), 应该可用
   - 需要实际烧录验证

5. **ramdisk init 脚本兼容性**
   - SDK 的 ramdisk init 脚本挂载 /dev/ubi0_0 (ubifs)
   - 改成 erofs 后, init 脚本需要能识别 cmdline 里的 rootfstype
   - 如果 SDK ramdisk 的 init 硬编码了 ubifs, 需要修改或替换

## 验证

```bash
./make-buildroot-release.sh -b hubv3l
# 产物: output/images/aml_upgrade_package.img
# 用 Amlogic USB Burning Tool 烧录验证
```
