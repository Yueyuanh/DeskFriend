# DeskFriend · Waveshare ESP32-S3-LCD-1.28

基于 Arduino、FreeRTOS 和 LVGL 9.4 的桌面表情演示。实物为 **240×240 GC9A01 非触摸版**，带 QMI8658 IMU 和 2 MB PSRAM；PlatformIO 借用 `waveshare_esp32s3_touch_lcd_128` 的板卡配置，不启用触摸。

网页只发送表情、字幕、进度、时钟、聊天分页和火柴人姿态等 JSON 状态。板端独立绘制动画，关闭浏览器后继续运行。上电显示 idle；未联网时外环红色呼吸，联网后恢复青色；晃动触发 dizzy 约 3 秒。

## 首次构建

安装 PlatformIO；首次生成字体和运行 QA 另需 Python 3.10+。在本目录执行：

```powershell
Copy-Item include/network_config.example.h include/network_config.h
# 编辑 network_config.h，填写自己的 2.4 GHz Wi-Fi 凭据。
python -m pip install -r requirements-dev.txt
python scripts/prepare_face_fonts.py
pio run
pio run -t upload --upload-port COM10
pio device monitor --port COM10 --baud 115200
```

字体从本机 Windows 安装生成，具体文件及限制见 [assets/fonts/README.md](assets/fonts/README.md)。凭据、字体二进制、构建目录和测试截图均已忽略提交。字体资源存在后，正常编译只需要 PlatformIO；网页、Three.js、字体一起编入固件，**无需 `uploadfs`**。

固件使用 UART0 115200，禁用 USB CDC；上传前关闭占用串口的监视器。串口输出访问地址。当前固定地址为 **http://192.168.137.73/**，网关 `192.168.137.1`；适用于 Windows 移动热点，需保持热点开启。其他网络请修改 `include/network_options.h` 的 IP、网关和掩码。

## 代码组织

| 路径 | 职责 |
|---|---|
| `src/main.cpp` | 串口与 FreeRTOS 任务创建 |
| `src/display_task.cpp` | 显示任务，独占 LCD、LVGL、SPI 和 IMU |
| `src/web_task.cpp` | Wi-Fi、HTTP 静态资源及状态 API |
| `src/face_animation.cpp`、`src/face_views.inc` | 表情、字幕、时钟、聊天和火柴人动画 |
| `src/mini_canvas.cpp` | Canvas 风格接口到 LVGL/ThorVG 的适配 |
| `src/face_state.cpp`、`src/device_state.cpp` | 任务间语义状态与本地晃动事件 |
| `src/face_frame.cpp` | 只读诊断截图的并发保护 |
| `src/DEV_Config.cpp`、`src/LCD_1in28.cpp`、`src/QMI8658.cpp` | Waveshare 示例衍生的硬件驱动 |
| `include/` | 接口、LVGL 配置、引脚和网络配置 |
| `DeskFriend_index.html`、`assets/` | 网页交互、本地 JS 依赖及字体说明 |
| `scripts/` | 资源生成、串口采集和真实板端验证 |
| `docs/LVGL_STATE_SYNC.md` | 实现流程、状态协议、问题修复和验证记录 |

显示任务运行于 Core 1，网络任务运行于 Core 0。LVGL 只能由显示任务调用；HTTP 不直接操作 LVGL。硬件引脚集中在 `include/DEV_Config.h`。

## 验证

最近一轮实物验证：73 组画面对照、14 项 HTTP 协议检查通过，用户确认晃动后约 3 秒恢复。复杂动画当前约支持 5～10 fps，浏览器与 LVGL 的文字/抗锯齿仍有差异，不能视为逐像素一致。详见 [验证记录](docs/LVGL_STATE_SYNC.md)。

手动回归需先关闭其他 DeskFriend 标签页，避免多个客户端覆盖状态。对照脚本会临时控制屏幕，结束后恢复 idle。

```powershell
python scripts/serial_capture.py --port COM10 --seconds 25
python scripts/test_face_protocol.py http://192.168.137.73
# Node.js 22+（内置 WebSocket）和 Chrome；可通过 CHROME_PATH 指定浏览器路径。
node scripts/verify_face_sync.mjs http://192.168.137.73
python scripts/compare_face_frames.py
```

结果写入 `.pio/face_qa/`。`/api/screen.rgb565` 只供测试读取板端画面，正常交互没有网页向板端传输像素的通道。不要把旧 `.pio/sync_test/` 的 mock 结果当作实物验证。

## 依赖与维护

PlatformIO 平台固定为本次实测的 Espressif32 8.0.0，LVGL 固定为 9.4.0。构建脚本会对项目本地 LVGL 应用矢量 alpha 修正和浮点字宽桥接；升级依赖需要重新审核补丁并做画面对照。Three.js 的许可保留于 `assets/vendor/LICENSE.three.txt`；Waveshare 驱动保留原版权/许可说明。

后续开发约定见 [AGENTS.md](AGENTS.md)。系统字体不随源码分发，生成脚本仅使用本地字体安装。
