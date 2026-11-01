# 文档索引

按用途分类存放，避免报告、日志、截图混在一起。

## reports/ — 结论文档

| 文件 | 内容 |
|---|---|
| `performance-report.html` | 各模型组合的帧率 / 单帧耗时 / 温度 / 固件占用实测报告 |
| `camera-no-soi-diagnosis.html` | OV5640 NO-SOI 取帧失败的完整排查记录与结论 |

## logs/ — 串口与实验原始日志

| 文件 | 内容 |
|---|---|
| `boot_check.log` / `boot_check2.log` | 启动流程验证（任务、模型、Wi-Fi、IP） |
| `detect.log` | 双模型时期的检测/识别耗时与 fps |
| `experiment-a.log` / `experiment-a2.log` | 实验 A：最小 RGB565 取帧 |
| `experiment-b.log` | 实验 B：JPEG 模式 + 传感器寄存器回读 |
| `experiment-c.log` | 实验 C：cam_hal 补丁验证 |
| `experiment-final-normal.log` | 恢复正常配置后的启动日志 |
| `experiment-wifi-led.log` | Wi-Fi 连接与状态灯行为（SSID 已脱敏） |

## data/ — 测量数据

| 文件 | 内容 |
|---|---|
| `bench.json` | 性能报告的原始采样数组（配置、fps、耗时、PSRAM、温度） |

## reference/ — 外部/上游参考

| 文件 | 内容 |
|---|---|
| `cam_hal.c.orig` | esp32-camera 上游 `cam_hal.c` 原件，用于补丁还原与 SHA-256 校验 |

## screenshots/ — 页面与画面截图（不入库）

个人画面，已被 `.gitignore` 忽略，仅保留在本地：

- `ui.png`、`ui-face-only.png`：网页控制台截图
- `capture.jpg`、`cap1-3.jpg`、`now.jpg`、`boxcheck.jpg`：摄像头抓拍/检测框验证
