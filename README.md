# DeskFriend

DeskFriend 是一个桌面陪伴设备项目，主仓库统一保存硬件设计、ESP32 固件、设计文档和原型资料；微信小程序保留为独立仓库，并通过 Git 子模块关联。

## 仓库结构

| 路径 | 内容 |
| --- | --- |
| `Docs/` | 项目设计文档、板卡资料和依赖恢复说明 |
| `ESP32/` | ESP32 固件、技术规范与参考工程 |
| `Mechanical/` | 机械结构、装配体和器件模型 |
| `Prototype/` | 原型验证资料 |
| `MiniProgram/DeskFriend/` | 微信小程序子模块 |
| `GIT_CHANGELOG.md` | 主仓库提交摘要，最新记录在最上方 |

## 获取完整仓库

```powershell
git clone --recurse-submodules https://github.com/Yueyuanh/DeskFriend.git
```

已经克隆主仓库但尚未取得小程序时，执行：

```powershell
git submodule update --init --recursive
```

## LCD 表情演示

[ESP32-S3-LCD-Test](ESP32/ESP32-S3-LCD/ESP32-S3-LCD-Test/README.md) 是 Waveshare ESP32-S3-LCD-1.28 **非触摸版**的 FreeRTOS/LVGL 工程：板端独立绘制面部动画，网页仅同步交互状态，支持字幕、进度、聊天、火柴人与 IMU 晃动触发。

首次构建需从配置模板填写本机 Wi-Fi 凭据，并从本地 Windows 字体生成资源；凭据和系统字体二进制不随仓库发布。构建、COM10 烧录和实际验证说明见工程 README 与 `docs/LVGL_STATE_SYNC.md`。

## 大型依赖

主仓库不保存可在线恢复的大型第三方库、重复的供应商压缩包或本机构建产物。相关版本和恢复步骤见：

- `Docs/Waveshare-ESP32-S3-LCD-1.28/DEPENDENCIES.md`
- `ESP32/ESP32-S3-CAM/reference/README.md`

PlatformIO 工程所需库由各工程的 `platformio.ini` 自动安装。请勿将 `.pio/`、`build/`、`node_modules/` 或 `miniprogram_npm/` 强制加入 Git。

## 维护约定

- 主仓库只维护项目自有内容；外部参考仓库优先记录来源和固定版本。
- 小程序代码在子模块仓库提交后，再在主仓库提交新的子模块指针。
- 每次提交主仓库时同步更新 `GIT_CHANGELOG.md`，保持倒序排列并概括关键变化。
- ESP32-S3-CAM 工程的硬件、FreeRTOS 和验证约束见 `ESP32/ESP32-SPEC/AGENTS.md`。
