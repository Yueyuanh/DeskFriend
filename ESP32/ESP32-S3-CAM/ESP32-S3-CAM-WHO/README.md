# DeskFriend 端侧视觉固件（ESP32-S3-CAM）

GOOUUU ESP32-S3-CAM + OV5640 的 ESP-IDF 固件：MJPEG 实时流、人脸检测与识别、
网页控制台、状态灯与芯片温度监控，所有推理都在板端完成。

- 芯片：ESP32-S3（16 MB QIO Flash，8 MB Octal PSRAM）
- 框架：ESP-IDF 6.1 + PlatformIO（`env:goouuu-s3-cam`）
- 模型：ESP-DL `MSRMNP_S8_V1`（人脸检测）+ `MFN_S8_V1`（人脸特征）
- 设备地址：`http://192.168.137.50/`（Windows 移动热点网段）

## 快速开始

```bash
# 1. 准备 Wi-Fi 凭据（真实文件不进仓库）
cp include/wifi_secrets.example.h include/wifi_secrets.h
#    然后填入 SSID / 密码

# 2. 编译、烧录、看串口
pio run -t upload
pio device monitor -b 115200
```

浏览器打开 `http://<设备IP>/` 即可看到实时画面与状态。

## 目录结构

```
ESP32-S3-CAM-WHO/
├── platformio.ini          构建入口：板型、flash、分区表、预编译脚本
├── CMakeLists.txt          ESP-IDF 工程声明
├── partitions.csv          分区表（factory 10 MB + storage 1 MB）
├── sdkconfig.defaults      摄像头/PSRAM/模型等 menuconfig 默认值
├── src/
│   ├── main.cpp            启动顺序：NVS → SPIFFS → 状态 → 各任务
│   ├── core/               跨任务共享状态（互斥锁保护的快照）
│   ├── vision/             摄像头驱动 + 取帧/检测/识别流水线
│   ├── web/                HTTP 服务 + 网页（index.html 及其生成的头文件）
│   ├── net/                Wi-Fi 连接与静态 IP
│   ├── board/              板载 WS2812 状态灯、芯片温度监控
│   ├── CMakeLists.txt      组件源文件清单与依赖
│   └── idf_component.yml   组件管理器依赖（esp32-camera、ESP-DL 模型）
├── include/                公共头文件（按模块分目录）
│   ├── app_config.h        摄像头引脚、静态 IP、凭据引入
│   ├── diag_config.h       摄像头诊断开关（正常为 0）
│   └── wifi_secrets.h      真实凭据（git 忽略）
├── tools/
│   ├── prebuild_models.py  PlatformIO 预构建钩子：预打包 ESP-DL 模型
│   ├── gen_html.py         把 index.html 包成 index_html.h
│   └── capture_serial.py   复位板子并抓取串口日志
└── docs/                   报告、日志、实验数据、截图（见 docs/README.md）
```

## 运行时任务

| 任务 | 位置 | 核心 / 优先级 | 职责 |
|---|---|---|---|
| Vision Task | `src/vision/vision_task.cpp` | core 1 / 5 | 取帧 → 发布 JPEG → 解码 → 人脸检测（250 ms）→ 识别（1 s） |
| Web Task | `src/web/web_task.cpp` | core 0 / 3 | HTTP 服务、MJPEG 分流、静态页 |
| LED 任务 | `src/board/led_status.cpp` | 默认 | 未联网红灯闪，联网后绿灯常亮 |
| 温度任务 | `src/board/temp_monitor.cpp` | 默认 | 每 2 s 采样芯片温度，保留 3 分钟曲线 |
| Wi-Fi 事件 | `src/net/wifi_manager.cpp` | 事件循环 | 连接、重连、静态 IP 上报 |

任务之间不直接调用，全部通过 `src/core/app_state.cpp` 的快照/请求接口通信。

## HTTP 接口

| 路径 | 方法 | 说明 |
|---|---|---|
| `/` | GET | 网页控制台 |
| `/stream` | GET | MJPEG 实时流（multipart/x-mixed-replace） |
| `/capture` | GET | 当前帧 JPEG |
| `/api/state` | GET | 设备状态、检测框、帧率、温度 |
| `/api/temps` | GET | 芯片温度历史 |
| `/api/inference?face=0\|1` | GET | 开启/停止人脸检测（画面继续上传） |
| `/api/enroll` | POST | 把当前人脸注册进人脸库 |
| `/api/clear` | POST | 清空人脸库 |

## 数据存储

- 人脸库：SPIFFS 分区 `storage`（0xA10000，1 MB）里的 `/spiffs/face.db`，
  只保存 MFN 特征向量，不保存照片。
- 分区表见 `partitions.csv`；刷固件不会清除人脸库，`pio run -t erase` 才会。

## 文档与记录

- 性能报告：`docs/reports/performance-report.html`
- 摄像头 NO-SOI 排障：`docs/reports/camera-no-soi-diagnosis.html`
- 串口日志 / 实验日志：`docs/logs/`
- 全部文件索引：`docs/README.md`

## 注意

- `include/wifi_secrets.h` 含真实热点凭据，已被 git 忽略；仓库里只保留
  `wifi_secrets.example.h` 模板。
- `docs/screenshots/` 含真人画面，同样被 git 忽略，不要提交。
