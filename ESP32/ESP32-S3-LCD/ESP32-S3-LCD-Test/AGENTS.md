# ESP32-S3-LCD-Test 项目约定

## 硬件

- 实物是 Waveshare ESP32-S3-LCD-1.28，GC9A01、240×240、2 MB PSRAM，**没有触摸功能**。
- PlatformIO 缺少非触摸版条目，借用 `waveshare_esp32s3_touch_lcd_128` 的芯片/上传配置；不得据此初始化 CST816S 或触摸输入。
- 屏幕驱动沿用 Waveshare Arduino 示例完整初始化：MOSI=11、SCLK=10、CS=9、DC=8、RST=12、BL=40，SPI 80 MHz。引脚统一在 `include/DEV_Config.h`。
- LCD 是单向写设备：使用 `SPIClass::writeBytes()`，不要用会覆盖发送缓冲的 `transfer()`。
- UART0 115200，禁用 USB CDC；已授权使用 COM10 编译、烧录、读取日志。

## 任务和资源

- `main.cpp` 只初始化串口和创建 FreeRTOS 任务。
- `Display Task`：Core 1，优先级 1，栈 49152 字节，独占 LVGL、LCD、SPI、QMI8658。绘制后至少让出 5 ms，禁止累积超期调度导致忙等。
- `Web Task`：Core 0，栈 6144；维护 Wi-Fi、HTTP 服务。HTTP 服务 Core 0，栈 16384。
- `FaceRequestState` 只保存语义状态；`DeviceState` 保存 IMU、联网状态和本地晃动事件。临界区内只复制数据，不能分配、绘制、串口打印或访问网络。
- LVGL 使用 `LV_OS_NONE`，只能由显示任务调用。大块像素/字形数据在 PSRAM，小型频繁访问对象优先 SRAM，并保留内部堆余量。
- 诊断截图经互斥锁复制完整 RGB565 帧，释放锁后才发送 HTTP；不得跨核操作 LVGL 对象。

## 表情和同步

- 网页 `DeskFriend_index.html` 是动画几何、颜色和时序的参考。`face_animation.cpp`、`face_views.inc` 移植对应公式；`MiniCanvas` 是 **LVGL 9.4 Canvas/ThorVG 矢量接口适配器**，不是原来的自制像素光栅器。
- 网页只 POST `/api/face` 语义状态：表情、字幕、动画时间/年龄、眨眼剩余时间、进度/变化率、日历、聊天分页、火柴人动作或摄像头关节。禁止添加画布像素、图片帧、视频帧、绘图命令上传。
- 收到状态后板子用自己的时钟继续动画，关闭网页仍运行。开机默认 idle 面部，不能改成 IMU 数据界面。
- 默认 idle 未联网时外环红色缓慢呼吸，联网后恢复原青色外环。`offline` 是视觉表情，**不能关闭 Wi-Fi**。
- IMU 检测连续两次加速度突变后，板端显示 dizzy 3 秒，冷却后才能再次触发；网页从 `/api/state` 读取本地事件同步预览。
- 字体由 `prepare_face_fonts.py` 生成，HTTP 和 LVGL 共用相同字体字节。内嵌 GB2312 常用汉字及页面字符，音乐符号/对勾补入 Segoe UI Symbol 轮廓；不承诺未收录的生僻字或彩色 Emoji。
- Windows 系统字体及生成的 `.ttf` 不提交；首次构建按 `README.md` 在本机生成。仅提交 `network_config.example.h`，不提交实际热点凭据。
- 不要调用 LVGL 9.4 的 `lv_vector_path_append_arc()` 来画任意角度：恰好 90° 附近可能产生退化末段/NaN。“网络断了”曾因此卡住绘制。使用 `MiniCanvas::arcPath()` 的等分三次贝塞尔段。
- `embed_assets.py` 对固定版本 LVGL 的矢量 alpha 255→254 截断做了窄范围修正；升级 LVGL 必须检查并重新验证。

## 网络

- 用户电脑 Windows 移动热点，2.4 GHz。凭据保存在忽略提交的 `include/network_config.h`，不要写进日志、文档或提交。
- 固定 IP 为 **192.168.137.73**，网关/DNS 192.168.137.1，掩码 255.255.255.0；统一修改 `include/network_options.h`。
- 网页、Three.js、监控模块和字体由 `embed_assets.py` 编入固件，不需要 uploadfs。
- `GET /api/state` 返回接收/显示序号、实测耗时、IMU 和本地事件；`GET /api/screen.rgb565` 仅用于读取板端诊断图像，`X-Frame-Seq` 标识画面状态。它不是网页下发画面通道。

## 验证与参考

- 修改后运行 `pio run`；烧录 `pio run -t upload --upload-port COM10`；用 `scripts/serial_capture.py` 有界读取日志，避免长期占用串口。
- `scripts/verify_face_sync.mjs http://192.168.137.73` 测试真实板端状态并保存网页/LCD 图像；`scripts/compare_face_frames.py` 生成误差与对照图。`.pio/sync_test/faces.log` 是旧 mock 服务日志，不能当作板端实测证据。
- 比较时关闭其他 DeskFriend 网页标签，避免两个客户端互相覆盖状态。对照要求同时固定时间、年龄、眨眼、进度、字体和日历。
- 报告实际测量的帧率和像素差异，不要把目标 30 fps 当成实测，也不要把浏览器/ThorVG 抗锯齿差异称为逐像素相同。
- 代码规范参考 `E:\MyProjects\DeskFriend\ESP32\ESP32-SPEC` 和 `E:\MyProjects\DeskFriend\ESP32\ESP32-S3-CAM\ESP32-S3-Dual`；屏幕底层参考 `E:\MyProjects\DeskFriend\Docs\Waveshare-ESP32-S3-LCD-1.28\ESP32-S3-LCD-1.28-Demo\Arduino`。
- 实现细节、协议和测量记录见 `docs/LVGL_STATE_SYNC.md`。
- 平台固定 Espressif32 8.0.0、LVGL 9.4.0；开发脚本依赖记录于 `requirements-dev.txt`。旧位图字模实现已移除，不要恢复 `glyphs.cpp`/`embed_glyphs.py`。
