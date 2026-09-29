# Zigbee2MQTT Package for Buildroot

这个包为 ThirdReality HubV3 添加了 Zigbee2MQTT 支持，使用定制的 BLZ radio type。

## 包含的组件

1. **zigbee-herdsman** - 定制版本 (分支: 3r_blz_10.0.8)
   - 仓库: https://github.com/thirdreality/zigbee-herdsman.git
   - 安装路径: `/opt/zigbee-herdsman`

2. **zigbee2mqtt** - 定制版本 (分支: 3r_blz_2.11.0)
   - 仓库: https://github.com/thirdreality/zigbee2mqtt.git
   - 安装路径: `/opt/zigbee2mqtt`

3. **mosquitto** - MQTT broker (版本: 2.0.20)
   - Buildroot 内置包

4. **nodejs** - JavaScript 运行时 (版本: 24.21.0)
   - Buildroot 内置包

## 配置

### 添加配置文件

将你的配置文件放在以下位置：

```
buildroot-external/package/zigbee2mqtt/configs/
├── configuration_zigate.yaml
└── configuration_blz.yaml
```

这些文件会被自动复制到 `/opt/zigbee2mqtt/data/` 目录。

### 添加外部转换器

将你的自定义转换器 JavaScript 文件放在：

```
buildroot-external/package/zigbee2mqtt/converters/
└── *.js
```

这些文件会被自动复制到 `/opt/zigbee2mqtt/data/external_converters/` 目录。

## 构建

1. 配置已经添加到 `thirdreality_hubv3_defconfig`
2. 执行构建：

```bash
make thirdreality_hubv3_defconfig
make
```

## 服务管理

Zigbee2MQTT 作为 systemd 服务运行：

```bash
# 启动服务
systemctl start zigbee2mqtt

# 停止服务
systemctl stop zigbee2mqtt

# 查看状态
systemctl status zigbee2mqtt

# 查看日志
journalctl -u zigbee2mqtt -f

# 开机自启
systemctl enable zigbee2mqtt
```

## 文件位置

- 程序目录: `/opt/zigbee2mqtt`
- 数据目录: `/opt/zigbee2mqtt/data`
- 配置文件: `/opt/zigbee2mqtt/data/configuration.yaml`
- 服务文件: `/usr/lib/systemd/system/zigbee2mqtt.service`

## 依赖关系

```
zigbee2mqtt
├── nodejs (24.21.0)
├── npm
├── mosquitto (MQTT broker)
└── zigbee-herdsman (定制版本)
```

## 注意事项

1. 构建过程需要联网下载 npm 包
2. 如果需要使用腾讯镜像加速，可以修改 `.mk` 文件中的 pnpm install 命令
3. zigbee-herdsman 必须在 zigbee2mqtt 之前构建
4. 确保有足够的存储空间（Node.js 和依赖包会占用较多空间）

## 故障排查

### 构建失败

1. 检查网络连接
2. 清理构建缓存: `make zigbee2mqtt-dirclean && make zigbee-herdsman-dirclean`
3. 重新构建: `make zigbee2mqtt-rebuild`

### 运行时问题

1. 查看服务日志: `journalctl -u zigbee2mqtt -n 100`
2. 检查 MQTT 连接: `systemctl status mosquitto`
3. 验证 Node.js 版本: `node --version`
4. 检查 USB 设备权限

## 版本信息

- zigbee-herdsman: 分支 3r_blz_10.0.8
- zigbee2mqtt: 分支 3r_blz_2.11.0
- mosquitto: 2.0.20
- nodejs: 24.21.0
