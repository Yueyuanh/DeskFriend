# ESP32-S3-CAM FreeRTOS 与硬件开发规范

版本：1.0  
适用工程：`ESP32-S3-CAM/ESP32-S3-Dual`  
维护方式：代码与本规范冲突时，先以实测和当前 `platformio.ini` 为准，再修订本文件，不要静默绕过规范。

## 1. 目标与边界

本规范统一 ESP32-S3-CAM 项目的 PlatformIO、Arduino-ESP32、FreeRTOS 任务、摄像头 DVP、NeoPixel、舵机、Wi-Fi 和 Web 控制实现。它描述当前工程已经采用的基线，同时标出参考工程参数，避免将参考代码误认为已接入功能。

当前工程已实现：

- `Servo Task`：GPIO47 pitch、GPIO21 yaw，20 ms 更新，指数平滑。
- `LED Task`：GPIO48 单颗 NeoPixel，显示 Wi-Fi 状态和表情颜色。
- `Web Task`：STA Wi-Fi、HTTP 80 端口、内嵌 `index.html`、舵机和 LED API。
- `loop()` 只负责 10 ms 让出 CPU；业务逻辑由 FreeRTOS 任务运行。

当前工程尚未接入：

- `esp_camera` 初始化、采集任务、帧队列、JPEG/RTSP 任务。
- 真实相机传感器的运行时验证。

## 2. 构建基线

当前 `platformio.ini` 的关键约定：

| 项目 | 值 | 说明 |
|---|---|---|
| Platform | `espressif32` | 由 PlatformIO 管理 |
| Board | `esp32-s3-devkitc-1` | Goouuu-CAM 无专用 PlatformIO board 定义，使用通用 S3 定义 |
| Framework | `arduino` | 使用 Arduino-ESP32 提供的 FreeRTOS |
| Flash | 16 MB | `board_upload.flash_size = 16MB` |
| 分区 | `default_16MB.csv` | 启用 16 MB Arduino 分区 |
| 内存模式 | `qio_opi` | 必须与板卡 Flash/PSRAM 实际硬件匹配 |
| PSRAM | `BOARD_HAS_PSRAM` | 允许相机帧和大缓冲放入 PSRAM |
| 上传/串口 | 921600 / 115200 | 上传速度与 monitor 速度分开 |
| 内嵌网页 | `index.html` | `board_build.embed_files`，通过 linker 符号发送 |

当前依赖：`Adafruit NeoPixel ^1.12.3`、`ESP32Servo ^3.0.9`。接入相机时应显式评估 `esp32-camera` 版本兼容性，不要仅复制参考项目的依赖。

## 3. FreeRTOS 总体模型

### 3.1 任务职责

| 任务 | 当前栈 | 优先级 | 核心 | 周期/阻塞 | 私有资源 |
|---|---:|---:|---:|---|---|
| Servo Task | 3072 | 1 | 1 | 20 ms `vTaskDelay` | 两个 `ESP32Servo` 对象 |
| LED Task | 3072 | 1 | 1 | 20 ms `vTaskDelay` | NeoPixel GPIO48 |
| Web Task | 8192 | 1 | 1 | 10 ms `vTaskDelay` | `WebServer`、Wi-Fi 状态 |
| Camera Capture（规划） | 起始 8192 | 5-8 | 0 | 阻塞于帧获取，失败退避 | `esp_camera`、帧缓冲 |
| Stream/Encode（规划） | 起始 8192 | 5-8 | 0 | 队列等待帧 | JPEG/RTSP socket |

核心亲和性不是装饰参数。默认沿用当前任务 pin 到 core 1；相机采集和编码新增前先测量 Wi-Fi、Web 和摄像头的负载，再决定是否将视频任务放到 core 0。不得无理由提高优先级。

### 3.2 任务模板

```cpp
void Task::run(void* arg) {
  (void)arg;
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    // 读取输入、处理私有外设、发布结果
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(PERIOD_MS));
  }
}

void Task::runTask(void* arg) { instance.run(arg); }
```

周期任务优先使用 `vTaskDelayUntil` 保持节拍；事件驱动任务优先使用队列、通知或事件组阻塞。ISR 中只调用 `FromISR` 版本，并在需要时请求切换。

### 3.3 共享状态与同步

- 单字节/布尔状态可用 `volatile`，但只适用于独立快照，不保证多个字段的一致性。
- 需要同时更新的结构体使用 `portMUX_TYPE`、FreeRTOS mutex 或临界区；临界区内不得调用 Wi-Fi、Serial、内存分配或阻塞 API。
- 生产者/消费者使用队列传递所有权；队列满时必须定义丢帧、覆盖旧帧或阻塞策略。
- 禁止多个任务同时调用同一个 `WebServer`、相机驱动或 NeoPixel 对象。

## 4. Goouuu ESP32-S3-CAM 相机配置

以下引脚来自仓库 `reference/ESP32-S3-Goouuu-Cam/Goouuu-CAM-RTSP/src/config.h` 与 `reference/ESP32S3/SGlass/include/cam_pin_ov5640.h`，属于本项目的 Goouuu-CAM 参考引脚表；接入前仍需以实际板卡丝印/原理图和探测日志验证。

| DVP 信号 | GPIO |
|---|---:|
| XCLK | 15 |
| SCCB SDA | 4 |
| SCCB SCL | 5 |
| D0/Y2 | 11 |
| D1/Y3 | 9 |
| D2/Y4 | 8 |
| D3/Y5 | 10 |
| D4/Y6 | 12 |
| D5/Y7 | 18 |
| D6/Y8 | 17 |
| D7/Y9 | 16 |
| VSYNC | 6 |
| HREF | 7 |
| PCLK | 13 |
| PWDN | -1 |
| RESET | -1 |

推荐相机初始化基线：`xclk_freq_hz = 14000000`、`frame_size = FRAMESIZE_QVGA`、PSRAM 存帧、PSRAM 存在时 `fb_count = 3`，无 PSRAM 降为 2。GC2145 参考方案使用 RGB565 或 YUV422 后软件 JPEG；传感器若支持硬件 JPEG，必须单独验证格式和缓冲占用。

相机配置必须集中在 `camera_config.h`/`camera_manager.*`，不得散落到任务、Web 路由或业务文件。初始化失败时打印 `esp_err_t`、传感器 PID、PSRAM 容量和最终格式；失败重试必须避免重复调用未释放的 `esp_camera_deinit()` 资源。

### 4.1 帧所有权协议

```cpp
camera_fb_t* fb = esp_camera_fb_get();
if (!fb) {
  // 记录错误并退避
  return;
}
// 队列只传递明确的所有权，或在此处完成编码/发送
esp_camera_fb_return(fb);
```

任何早退路径都必须归还帧。若队列传递 `camera_fb_t*`，消费者归还；若复制为独立 JPEG buffer，复制成功后立即归还原帧，并在发送失败时释放 JPEG buffer。

### 4.2 性能基线

优先从 QVGA、低延迟、有限帧缓冲开始，再逐步提高分辨率。参考 GC2145 项目报告 QVGA 下 RGB565 约 12 FPS、YUV422 约 17 FPS，但这不是当前硬件的保证值。每次改变格式、XCLK、分辨率或 `fb_count` 都要重新记录实际 FPS、堆/PSRAM 峰值和丢帧数。

## 5. 已确认外设引脚与电气约束

| 外设 | GPIO | 约束 |
|---|---:|---|
| NeoPixel 数据 | 48 | 当前为 1 颗，`NEO_GRB + NEO_KHZ800`，亮度默认 64 |
| Pitch servo | 47 | 脉宽 500-2500 us，初始 90 度 |
| Yaw servo | 21 | 脉宽 500-2500 us，初始 90 度 |

舵机必须使用独立、足够的 5 V 供电并与 ESP32 共地；不得由 GPIO 供电。修改相机引脚时必须检查是否与舵机、NeoPixel、USB/JTAG 或启动配置冲突。

## 6. 网络与 Web 规范

- Wi-Fi 凭据不得硬编码提交；当前源码中的占位凭据必须在产品化前移出源码。
- `WebServer` 只由 Web Task 调用；路由处理函数只做参数校验、更新命令或生成快照，不执行舵机渐变、相机编码等长任务。
- 所有输入都限制范围：舵机 0-180，颜色 0-255；新增参数必须拒绝非法值并返回明确 HTTP 状态码。
- Wi-Fi 断线时 Web Task 必须持续让出 CPU，并使用退避重连；LED 状态只能通过受保护的状态快照通知 LED Task。
- API 行为变更必须同步更新 `index.html`、串口日志说明和测试清单。

## 7. 内存、栈与可靠性

- 大型帧缓冲、JPEG buffer、JSON/HTML 大对象优先放 PSRAM；任务栈和短生命周期控制结构放内部 RAM。
- 禁止在高频循环中无界 `String` 拼接或反复分配；长期运行路径要关注堆碎片。
- 每个新增任务至少记录 `uxTaskGetStackHighWaterMark()`；相机/编码任务运行后建议保留 25% 以上栈余量。
- 看门狗采用“任务有进度才喂狗”的原则；禁止用无限喂狗掩盖死锁、队列阻塞或帧泄漏。
- 初始化失败要可诊断：打印模块、错误码、关键配置和恢复动作；无法恢复时进入明确的降级状态。

## 8. 目录与代码组织

```text
ESP32-S3-Dual/
  platformio.ini
  src/main.cpp                 # 启动顺序和任务创建
  src/*_task.cpp               # 一个任务一个实现边界
  include/*_task.h             # 公开句柄、命令和状态声明
  include/camera_config.h      # 相机引脚和采集参数（接入后新增）
  index.html                   # Web UI
```

任务类提供 `begin()`、实例 `run()` 和静态 `runTask()` 入口。`main.cpp` 只做串口、硬件 begin、任务创建和最小 loop，不承载业务循环。

## 9. 验证清单

提交前至少执行：

1. `pio run`
2. `pio run --target upload`（接线和端口确认后）
3. `pio device monitor -b 115200`
4. 检查启动日志：PSRAM、相机 PID/格式、Wi-Fi IP、任务创建结果。
5. Wi-Fi 断开/恢复、Web API 边界值、舵机归中、LED 状态和长时间运行测试。
6. 接入相机后检查：帧归还计数、队列满行为、FPS、丢帧、堆/PSRAM 峰值和所有任务栈水位。

## 10. 当前已知风险与变更记录

- 当前 `ESP32-S3-Dual` 尚未包含相机驱动；本文件的相机部分是经仓库参考工程整理的接入基线，不代表已通过本项目硬件测试。
- 参考文件存在编码异常，阅读时以数值配置和源码为准；不要复制其中乱码文本。
- 当前 Web 任务源码包含示例 Wi-Fi 字符串，必须在部署前替换为安全的运行时配置。
- 任何新验证结果应在此处追加日期、硬件/传感器型号、构建版本、结论和回滚方式。
