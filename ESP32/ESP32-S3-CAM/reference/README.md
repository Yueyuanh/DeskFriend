# ESP32-S3-CAM 参考工程

本目录用于存放外部参考代码。`ESP32-S3-Goouuu-Cam` 是可在线取得的上游仓库，因此不复制进 DeskFriend 主仓库，也不作为第二个子模块维护。

需要恢复时，在本目录执行：

```powershell
git clone https://github.com/zhuhai-esp/ESP32-S3-Goouuu-Cam.git
```

项目当前参考的上游版本为 `3ffc1e9`（`fix web cam options`）。若直接拉取了更新版本，请先核对相机引脚、Flash/PSRAM 配置和 API 差异，再将实现移植到项目源码中。

同目录的 `ESP32S3/` 是项目现有参考资料，会随主仓库保存；其中 `.pio/`、`.vscode/` 等本机构建和编辑器产物由根目录 `.gitignore` 排除。
