# Waveshare ESP32-S3-LCD-1.28 依赖说明

本目录保留项目使用的板卡资料、示例源码和定制配置。为控制主仓库体积，以下可在线恢复的内容不会提交：

- `ESP32-S3-LCD-1.28-Demo.zip`：官方完整示例压缩包；
- `ESP32-S3-LCD-1.28-3D.zip`：已解压在同目录的 3D 模型压缩包；
- `ESP32-S3-LCD-1.28-Demo/Arduino/libraries/lvgl/`：LVGL `8.3.8`；
- `ESP32-S3-LCD-1.28-Demo/Arduino/libraries/TFT_eSPI/`：TFT_eSPI `2.5.34`；
- `ESP32-S3-LCD-1.28-Demo/Firmware/`：官方预编译固件；
- 示例目录中的 `build/`：Arduino 编译产物。

## 恢复方法

1. 打开 [Waveshare 官方文档](https://docs.waveshare.net/ESP32-S3-LCD-1.28)，在“资料/示例程序”处下载完整 Demo，需要原始 3D 包时也从该页面获取。
2. 将 Demo 解压到当前目录，保持 `ESP32-S3-LCD-1.28-Demo` 这一目录名。
3. 若只需要 Arduino 库，可通过 Arduino Library Manager 安装 `lvgl` `8.3.8` 和 `TFT_eSPI` `2.5.34`。
4. 保留仓库内的 `Arduino/libraries/TFT_eSPI_Setups/`，其中包含本板卡使用的定制配置。

这些目录即使存在于本机也会被根目录 `.gitignore` 排除，不影响本地编译和查阅。
