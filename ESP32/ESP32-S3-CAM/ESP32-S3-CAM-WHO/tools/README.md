# 工具脚本

| 脚本 | 用途 |
|---|---|
| `prebuild_models.py` | PlatformIO 预构建钩子（`extra_scripts`）。在 Ninja 启动前把 ESP-DL 模型打包成 `*.espdl.S`，避免干净构建时找不到模型文件。由 PlatformIO 自动调用，无需手动运行。 |
| `gen_html.py` | 把 `src/web/index.html` 包成 `src/web/index_html.h` 里的 raw string。改完网页后运行一次。 |
| `capture_serial.py` | 通过 DTR/RTS 复位板子并抓取 N 秒串口输出（默认 COM12，25 秒）。 |

## 用法

```bash
# 网页改动后重新生成固件内置页面
python tools/gen_html.py

# 复位并抓取 30 秒串口日志（参数：端口 秒数）
python tools/capture_serial.py COM12 30 > docs/logs/boot_check.log
```

`prebuild_models.py` 使用当前 PlatformIO 核心的 `$PYTHONEXE` 与
`$PROJECT_PACKAGES_DIR/tool-cmake`，因此工程目录里不需要额外的工具链副本。
