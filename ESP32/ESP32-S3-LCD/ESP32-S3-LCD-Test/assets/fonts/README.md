# 本地字体资源

固件和网页共享三个 TrueType 字体子集：`ui-bold.ttf`、`ui-regular.ttf`、`clock.ttf`。它们由脚本从本机 Windows 字体生成，不提交系统字体二进制或生成的子集。

在项目目录执行：

```powershell
python -m pip install -r requirements-dev.txt
python scripts/prepare_face_fonts.py
```

脚本读取 `C:/Windows/Fonts/` 中的 Microsoft YaHei（`msyh.ttc`、`msyhbd.ttc`）、Arial Rounded MT Bold（`ARLRDBD.TTF`）和 Segoe UI Symbol（`seguisym.ttf`）。缺少文件时须先在本机合法安装相应字体。现有脚本默认适用于 Windows；其他系统需要提供同样的本地字体并调整源路径。

生成内容包括 GB2312 常用汉字、页面字符、时钟数字及音符/对勾轮廓。`scripts/embed_assets.py` 将这些文件编入固件并通过 HTTP 提供给浏览器。字体改变后需要重新编译，并重新运行画面对照；`MiniCanvas` 中的文字基线校准也需要复核。
