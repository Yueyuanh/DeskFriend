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
│   ├── main.cpp            启动顺序：NVS → SPIFFS → 状态 → 各任务 → USB UVC
│   ├── core/               跨任务共享状态（互斥锁保护的快照）
│   ├── vision/             摄像头驱动 + 取帧/检测/识别流水线
│   ├── web/                HTTP 服务 + 网页（index.html 及其生成的头文件）
│   ├── net/                Wi-Fi 连接与静态 IP
│   ├── board/              板载 WS2812 状态灯、芯片温度监控
│   ├── usb/                UVC 有线摄像头（把最新 JPEG 交给 USB 主机）
│   ├── CMakeLists.txt      组件源文件清单与依赖
│   └── idf_component.yml   组件管理器依赖（esp32-camera、ESP-DL 模型）
├── components/
│   └── usb_device_uvc/     内置的 usb_device_uvc 1.3.1（见“USB 有线摄像头”）
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
| TinyUSB | `components/usb_device_uvc` | core 0 / 5 | USB 设备栈，处理枚举与控制请求 |
| UVC 视频 | `components/usb_device_uvc` | core 0 / 4 | 按主机帧率从 app_state 取最新 JPEG 发送 |

任务之间不直接调用，全部通过 `src/core/app_state.cpp` 的快照/请求接口通信。

## USB 有线摄像头（UVC）

板子有两个 Type-C：UART 口（CH340，烧录/日志）和 OTG 口。把 **OTG 口** 插到电脑，
系统会把板子识别成一块 UVC 摄像头（设备名 `DeskFriend Camera`），用 Windows「相机」、
OBS 或 VLC 直接打开即可，不需要驱动，也不需要 Wi-Fi。

主机可以在相机设置里选择四种分辨率（默认 QVGA）：

| 模式 | 分辨率 | 广告帧率 | 说明 |
|---|---|---|---|
| QVGA | 320×240 | 15 fps | 默认；Wi-Fi 调试与推理最快 |
| HVGA | 480×320 | 15 fps | 3:2 画幅 |
| VGA | 640×480 | 10 fps | 4:3；细节和带宽的平衡点 |
| SVGA | 800×600 | 5 fps | 后期处理/抓帧用，推理会明显变慢 |

- 实现：`src/usb/uvc_camera.cpp`。UVC 的 `fb_get` 只从 `app_state` 复制最新 JPEG，
  不直接碰摄像头驱动。
- 切换分辨率：UVC 主机 commit 新模式时，`start_cb` 把目标 `framesize_t` 投递给
  `app_state`；视觉任务在两帧之间（此时不持有 frame buffer）`esp_camera_deinit()`
  + `esp_camera_init()`，其余任务随之看到新的 `frame_w/frame_h`。Wi-Fi `/stream`、
  人脸检测和识别共用同一个传感器，所以它们会一起切到新分辨率；网页上的检测框
  按帧尺寸缩放，分辨率一栏会显示当前值。
- 主机停止取流（关闭相机应用）后会自动切回 QVGA，让 Wi-Fi 和推理恢复帧率。
- 在 VGA/SVGA 下，人脸检测改为约 1 秒一次，JPEG 解码只发生在检测帧上；其余帧
  只发布画面，所以推流帧率不会被推理拖慢（VGA 实测仍约 11 fps）。
- USB Full-Speed 等时传输上限约 512 KB/s，上表帧率是实际可用值；更大的模式帧率
  会被带宽和 ESP-DL 解码/推理耗时共同压低。
- 只广告 MJPEG：`usb_device_uvc` 把一帧 JPEG 直接放进 128 KB 的传输缓冲区
  （SVGA 在驱动里最多 800×600/5 = 96 KB）。
- 组件来源：`espressif/usb_device_uvc` 1.3.1 内置在 `components/usb_device_uvc`。
  上游 `CMakeLists.txt` 用 `target_sources(... PUBLIC ...)` 把 `usb_descriptors.c`
  挂成 tinyusb 的 INTERFACE 源，PlatformIO 会为同一文件生成两个同名目标而报
  “Two environments with different actions…”，因此本地改为 `PRIVATE`（文件仍编译
  进 tinyusb 库，行为不变）。
- 后续计划：复合设备再加一路 CDC，把芯片温度、识别角度等状态回传给电脑端程序。

## 灵敏度调节

网页上的「灵敏度」卡片有两个滑块，改完立即生效并写入 NVS，重启后保留：

| 滑块 | 范围 | 默认 | 作用 |
|---|---|---|---|
| 检测阈值 | 0.20–0.70 | 0.40 | 同时作用于检测器的 MSR（候选）和 MNP（精筛）两段 |
| 识别阈值 | 0.30–0.90 | 0.50 | MFN 特征与已注册人脸的余弦相似度门限 |

- 数值**越小越灵敏**：检测阈值调低能抓到侧脸、远处和小脸；识别阈值调低，
  没正对镜头时也更容易认成已注册的人，但太低会认错。
- 识别阈值需要能低于 0.50，而组件自带的 `HumanFaceRecognizer` 把 0.5 写死在
  构造函数里、没有 setter，所以 `src/vision/face_recognizer.cpp` 直接用
  `HumanFaceFeat` + `dl::recognition::DataBase` 重新实现了一遍查询与注册，
  行为与原类一致，只是阈值由调用方传入。
- 阈值保存在 NVS 命名空间 `tuning`（`det_thr` / `rec_thr`，单位为千分之一），
  刷固件不会覆盖，恢复默认请点网页上的「恢复默认」或清 NVS。

## HTTP 接口

| 路径 | 方法 | 说明 |
|---|---|---|
| `/` | GET | 网页控制台 |
| `/stream` | GET | MJPEG 实时流（multipart/x-mixed-replace） |
| `/capture` | GET | 当前帧 JPEG |
| `/api/state` | GET | 设备状态、检测框、帧率、温度 |
| `/api/temps` | GET | 芯片温度历史 |
| `/api/inference?face=0\|1` | GET | 开启/停止人脸检测（画面继续上传） |
| `/api/tuning?detect=0.4&recognize=0.5` | GET | 调整检测/识别阈值并存 NVS；`?reset=1` 恢复默认 |
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
