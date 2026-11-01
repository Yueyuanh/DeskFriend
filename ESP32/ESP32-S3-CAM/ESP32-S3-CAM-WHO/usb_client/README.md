# DeskFriend USB 客户端

在有线（OTG）连接下使用 ESP32-S3 摄像头：实时画面、人脸框、芯片温度曲线，
以及向下位机发送舵机角度和任意文本。**全程不依赖 Wi-Fi**，也没有网页端，
插上线就能跑。

## 它和网页端的区别

| 能力 | 网页端 | 本客户端 |
| --- | --- | --- |
| 画面来源 | Wi-Fi MJPEG (`/stream`) | USB UVC（MJPG） |
| 设备信息 / 温度 | `/api/state` | USB CDC，每 500 ms 一行 JSON |
| 人脸框 | 端侧检测后由网页叠加 | 同一份检测结果，经 CDC 下发后叠加 |
| 控制（人脸开关、阈值、注册、清空） | HTTP API | CDC 文本命令 |
| 舵机 / 自定义数据 | — | CDC 文本命令，下位机串口打印 `RX:` |

需要 Wi-Fi 的功能（如网页本身）在本客户端里都不存在，因此设备没有连上
热点时同样可用。

## 运行环境

- Python 3.10–3.14（Windows 10/11、macOS 12+、主流 Linux 均可）。
  PySide6 6.12 起才提供 Python 3.14 的 wheel；更老的 Python 也能用，但版本会
  按 `requirements.txt` 跳到 6.12。
- 系统里要有 USB 摄像头驱动；Windows 上走 DirectShow（Media Foundation 打开
  被占用的设备会卡十几秒，所以不用），macOS 用 AVFoundation，Linux 用 V4L2
  （用户需在 `video` 组里）。
- 设备侧需要带 CDC 复合功能的固件：一块板子同时出现 **UVC 摄像头** 和
  **一个串口**。

## 安装

```bash
cd ESP32/ESP32-S3-CAM/ESP32-S3-CAM-WHO/usb_client
python -m venv .venv

# Windows
.venv\Scripts\activate
# macOS / Linux
source .venv/bin/activate

pip install -r requirements.txt
```

> 国内网络可以用镜像加速，例如
> `pip install -i https://pypi.tuna.tsinghua.edu.cn/simple -r requirements.txt`。
> 镜像同步有延迟时可能还没有 PySide6 6.12（Python 3.14 需要它），那就去掉
> `-i` 用官方源安装。

## 运行

```bash
python app.py
```

界面分三块：

- **左侧画面**：UVC 实时画面，按窗口等比缩放，窗口怎么拉伸都不会变形。
- **右侧状态**：摄像头 / 数据链路 / 分辨率 / 帧率 / 芯片温度 / 人脸数 /
  识别结果 / 相似度 / 人脸库数量；下方是分辨率、串口选择、人脸开关、
  注册、清空和「显示人脸框」勾选框（关掉就不再画框）。
- **底部**：芯片温度曲线（最近 3 分钟），以及舵机角度和自定义文本的发送框。

串口默认「自动」，会按 Espressif 的 USB VID (`0x303A`) 找设备；插拔后
会自动重连。如果机器上还有别的 Espressif 设备，可以在下拉框里手动指定
`COMx` / `/dev/ttyACMx`。

摄像头也默认「自动」：自动模式只认 ESP32 独有的 480×320 模式，笔记本自带的
摄像头给不出这个尺寸，所以不会被误开（也不会亮灯）；没找到就每秒重试，插上
OTG 线后画面自动出现。要用别的摄像头时在「摄像头」下拉框里手动指定索引。

## 发送的命令

每行一条，UTF-8 文本。客户端自己会用到的：

| 命令 | 作用 |
| --- | --- |
| `FACE 1` / `FACE 0` | 启动 / 停止端侧人脸检测 |
| `THR <detect> <recognize>` | 设置检测 / 识别阈值，例如 `THR 0.4 0.5` |
| `ENROLL` | 把下一张检测到的人脸注册进库 |
| `CLEAR` | 清空人脸库 |
| `PING` | 连通性测试，回复 `PONG` |
| `SERVO <水平> <俯仰>` | 舵机角度，例如 `SERVO 90 120` |

其他任何文本都会原样转发：下位机 UART 控制台打印 `RX: <原文>`，并经由
CDC 回一行 `ACK <原文>`。这就是验证数据链路的方式——客户端底部的
「← ACK ...」和串口监视器里的 `RX:` 应该一一对应。舵机命令目前只打印，
等接上舵机后在下位机里按 `SERVO` 解析即可。

## 设备 → 客户端的数据

每 500 ms 一行 JSON，例如：

```json
{"t":43.2,"fps":10.9,"w":320,"h":240,"id":-1,"sim":0.00,"db":0,"faces":[[120,64,80,80,0]]}
```

- `t`：芯片温度（°C），取不到时为 `null`
- `w`/`h`：当前画面尺寸
- `id`/`sim`：识别到的人脸库序号和相似度；`-1` 表示未识别
- `db`：人脸库中的特征数量
- `faces`：`[x, y, w, h, score]`，坐标是画面像素，客户端按缩放比例叠加

## 打包分发

<!-- 各平台各打各的，不要用 -F 之外的参数混用 -->
**Windows**

```powershell
powershell -ExecutionPolicy Bypass -File build_exe.ps1
```

脚本会创建 `.venv`（如果还没有）、安装依赖并打包，产物是
`dist\DeskFriend-Camera.exe`，单文件，双击即用。等价的原始命令：

```powershell
pip install pyinstaller
pyinstaller -F -w -n DeskFriend-Camera app.py
```

`-w` 去掉控制台窗口；如果打包后的程序启动不了，先去掉 `-w` 从命令行运行看报错。

**macOS**

```bash
pip install pyinstaller
pyinstaller -F -w -n DeskFriend-Camera app.py
```

首次打开需要在「系统设置 → 隐私与安全性 → 摄像头」里允许。
也可以用 `--windowed --name "DeskFriend Camera" --osx-bundle-identifier ...` 打成 `.app`，
再压缩成 zip 分发。

**Linux**

```bash
pip install pyinstaller
pyinstaller -F -w -n deskfriend-camera app.py
```

分发时注意目标机器的 glibc 版本不低于打包机器；跨发行版分发推荐在较老的
基础镜像里打包。

> PySide6 单个平台约 100 MB 以上，打包文件偏大是正常的。不同平台必须各自
> 打包，不能在 Windows 上打出 macOS 的可执行文件。

## 常见问题

- **只看到摄像头、没有串口**：确认固件是带 CDC 的最新版本；Windows 设备
  管理器里应出现「USB 串行设备 (COMx)」。若显示黄色感叹号，重新插拔或用
  ESP-IDF 自带的 USB CDC 驱动。
- **打不开摄像头**：确认 USB 设备管理器里板子显示为「摄像头」而不是带黄色
  感叹号的复合设备（Code 10 说明固件描述符有问题，重新烧录最新固件）；再
  关闭其他占用摄像头的程序（浏览器标签、相机应用）。「自动」模式找不到
  ESP32 时不会退回笔记本摄像头，可在「摄像头」下拉框里手动选索引，再切到
  320×240 确认链路。
- **画面卡顿**：MJPG 在 USB 全速下带宽有限，800×600 帧率会明显下降，
  这是正常的。日常用 320×240 或 640×480。
- **人脸框不动**：框来自端侧检测结果，每 500 ms 更新一次；检测本身约 4 Hz。
- **没有温度曲线**：`t` 为 `null` 说明温度传感器尚未出值，等几秒或检查固件
  日志里的 tsens 初始化。
