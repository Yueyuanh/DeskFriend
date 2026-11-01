# Git Change Log

本文件面向项目维护者，按时间倒序记录主仓库的关键提交信息。

## 2026-11-02 - `841699c` `docs(agents): move project conventions into AGENT.md`

**类型：** 工程约定
**范围：** `ESP32/ESP32-S3-CAM/ESP32-S3-CAM-WHO`

- 变更记录、仓库安全、常用命令等约定迁入 `AGENT.md`，`CLAUDE.md` 只保留 `@AGENT.md` 引入，其他 agent 工具也能读到同一份规则。

## 2026-11-02 - `751eec6` `feat(usb-client): add desktop client over UVC and CDC`

**类型：** 桌面客户端
**范围：** `ESP32/ESP32-S3-CAM/ESP32-S3-CAM-WHO/usb_client`

- 新增 PySide6 客户端：UVC 实时画面、端侧人脸框叠加、芯片温度曲线、设备状态，以及经 CDC 发送舵机角度/自定义文本的输入框；全程不依赖 Wi-Fi。
- 自动模式用 ESP32 独有的 480×320 模式认设备，命中后在同一句柄上切到用户选择的分辨率；Windows 只用 DirectShow，避免 Media Foundation 在设备被占用时卡十几秒。
- 窗口按屏幕可用区域定尺寸并居中，状态栏放进滚动区域，125%/200% 缩放下不再把右栏顶出屏幕或让文字重叠。
- `build_exe.ps1` 打包单文件 exe（约 97 MB），README 写明 macOS/Linux 打包方式和常见问题。

## 2026-11-02 - `f09ed92` `feat(usb): add CDC telemetry and command channel`

**类型：** ESP32-S3-CAM 固件
**范围：** `ESP32/ESP32-S3-CAM/ESP32-S3-CAM-WHO`（`components/usb_device_uvc`、`src/usb`、`include/usb`）

- UVC 之后复合一路 CDC ACM，同一根 OTG 线上电脑同时看到摄像头和一个串口；每 500 ms 下发一行 JSON（温度、分辨率、帧率、人脸框、识别结果），主机文本命令在 UART 打印 `RX:` 并回 `ACK`/`PONG`。
- 修复复合设备无法启动（设备管理器 Code 10）：`TUD_CDC_DESC_LEN` 已包含 8 字节 IAD，早先又手写了一个 IAD，导致配置描述符出现两个关联描述符。
- 实机验证：UVC 摄像头与 COM14 同时枚举，`PING`/`SERVO` 命令有应答，遥测持续收到（约 51.6 °C、11.1 fps）。

## 2026-11-02 - `846fe6a` `feat(vision): web-tunable thresholds and USB resolution sync`

**类型：** 视觉流水线
**范围：** `ESP32/ESP32-S3-CAM/ESP32-S3-CAM-WHO`

- 网页「灵敏度」滑块调整检测阈值（0.20–0.70）与识别阈值（0.30–0.90），写入 NVS，重启保留；新增 `/api/tuning`。
- 识别改为直接用 `HumanFaceFeat` + `dl::recognition::DataBase` 查询，绕开 `HumanFaceRecognizer` 写死的 0.5 相似度门限。
- 视觉任务消费 USB 主机提交的帧尺寸请求，在帧间重新初始化传感器并发布新尺寸，Wi-Fi 推流、检测和网页叠加一起切换；只在检测帧上解码，VGA 从约 2.5 fps 提升到约 11 fps。

## 2026-11-02 - `742d38d` `feat(usb): add UVC webcam with host-selectable frame sizes`

**类型：** ESP32-S3-CAM 固件
**范围：** `ESP32/ESP32-S3-CAM/ESP32-S3-CAM-WHO`

- 板子在 OTG Type-C 口上枚举成 UVC 摄像头，电脑无需 Wi-Fi、无需驱动即可打开 MJPEG 画面。
- 内置 `espressif/usb_device_uvc` 1.3.1；上游 CMakeLists 把 `usb_descriptors.c` 作为 INTERFACE 源发布，PlatformIO 会重复编译同一文件而中止，改为 PRIVATE 后行为不变。
- 广告 QVGA/HVGA/VGA/SVGA 四种 MJPEG 模式（主机在相机设置里选择）；`camera_manager` 增加 `set_framesize()`，在帧间 deinit/init，失败时回退 QVGA；`app_state` 增加请求槽，保证只有视觉任务操作传感器。

## 2026-11-02 - `25acdfe` `refactor(esp32-cam): organize sources, docs and tools by module`

**类型：** ESP32-S3-CAM 固件工程整理
**范围：** `ESP32/ESP32-S3-CAM/ESP32-S3-CAM-WHO`

- `src/` 与 `include/` 按模块拆分：core（共享状态）、vision（摄像头与人脸流水线）、web（HTTP 与页面）、net（Wi-Fi）、board（状态灯与温度）。
- 构建脚本 `prebuild_models.py` 移入 `tools/`，`platformio.ini` 的 `extra_scripts`、`gen_html.py` 的输入输出路径同步更新；新增工程、docs、tools 三份 README。
- `docs/` 按 reports / logs / data / reference / screenshots 分类，报告内引用的路径同步修改；截图目录整目录加入 git 忽略。
- 删除 PlatformIO 默认的空 `lib/`、`test/` 脚手架，无用的 `include/README`；固件行为与接口未改动。
- 整理后完成一次完整编译：Flash 34.2%、RAM 15.9%。期间发现并重新安装了不完整的 esp-dl 3.3.14 组件，`dependencies.lock` 未变化。

## 2026-11-02 - `1ce25e5` `chore: ignore generated UI screenshots`

- 将网页截图 `docs/ui*.png` 加入项目忽略规则，避免把含真人画面的截图提交到公开仓库。

## 2026-11-02 - `722eaa2` `refactor(vision): drop pedestrian detection, keep face recognition only`

- 移除 PICO 行人检测模型、人体计数和对应网页开关，只保留 MSRMNP 人脸检测 + MFN 人脸识别。
- 固件从 4,031,683 B（38.4%）降到 3,588,631 B（34.2%）；空场景推理从 5.55 fps / 178 ms 提升到 11.1 fps / 47 ms，PSRAM 空闲增加约 1.3 MB。
- 带人体检测的完整版本保留在分支 `feat/face-human-switches`；实测数据见 `docs/reports/performance-report.html`。

## 2026-11-01 - `feat(lcd): add LVGL face state synchronization`

**类型：** LCD 固件与网页交互
**范围：** `ESP32/ESP32-S3-LCD/ESP32-S3-LCD-Test`、主仓库入口文档

- 增加非触摸 Waveshare 1.28 英寸板卡的 FreeRTOS/LVGL 9.4 工程，网页仅发送语义状态，板端独立绘制表情、字幕、进度、时钟、聊天与火柴人动画。
- 修复“网络断了”圆弧绘制卡死、音符缺字和文字位置；加入固定 IP、离线红色呼吸环、晃动触发 3 秒 dizzy，保留后台 IMU。
- 清理旧位图字模和模板文件，固定平台依赖，补齐首次构建、字体生成、状态协议和真实板端验证说明；排除 Wi-Fi 凭据、系统字体二进制及本机缓存。
- 此前固件已烧录 COM10；73 组真实板端画面对照和 14 项 HTTP 协议检查通过，用户确认晃动效果约 3 秒恢复。整理后再次完成 PlatformIO 编译。
- 当前复杂画面约支持 5～10 fps，浏览器与 LVGL 字形/抗锯齿仍存在差异，实测边界已记录。

## 2026-11-01 - `chore: consolidate DeskFriend repository`

**类型：** 仓库结构基线  
**范围：** 主仓库、子模块、依赖管理、项目文档

- 梳理主仓库目录，将文档、ESP32 固件、机械设计和原型资料纳入统一版本管理。
- 将微信小程序登记为 `MiniProgram/DeskFriend` 子模块，继续使用独立仓库维护。
- 增加根级过滤规则，排除 PlatformIO/Arduino 构建产物、前端依赖、可在线恢复的大型第三方库和重复压缩包。
- 在大型依赖原位置增加恢复说明，保留版本、下载入口和注意事项。

## 2026-10-29 - `8f956df`

**提交者：** Yueyuanh  
**主题：** `first commit`

- 建立 DeskFriend 主仓库。
- 添加初始 `README.md`。

---

维护约定：每次主仓库提交后在本文件顶部新增一节，至少写明日期、短哈希、提交主题和关键变化；合并提交应说明包含的功能或修复，不只记录分支名。子模块自身的提交历史由小程序仓库维护。
