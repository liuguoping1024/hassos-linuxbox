# Zigbee2MQTT 配置文件

将你的 Zigbee2MQTT 配置文件放在这个目录中。

## 配置文件

需要添加以下配置文件：

1. `configuration_zigate.yaml` - ZiGate 适配器配置
2. `configuration_blz.yaml` - BLZ 适配器配置

这些文件将在构建时自动复制到目标系统的 `/opt/zigbee2mqtt/data/` 目录。

## 配置示例

```yaml
# configuration_blz.yaml 示例
homeassistant: true
permit_join: false
mqtt:
  base_topic: zigbee2mqtt
  server: mqtt://localhost:1883
serial:
  port: /dev/ttyUSB0
  adapter: blz
advanced:
  log_level: info
  enable_external_js: true
  channel: 11
  pan_id: 6754
  network_key: GENERATE
frontend:
  port: 8080
  host: 0.0.0.0
```

## 注意事项

- 确保配置文件格式正确（YAML）
- 根据实际硬件修改 serial port
- 建议不要在配置文件中包含敏感信息（如密码）
