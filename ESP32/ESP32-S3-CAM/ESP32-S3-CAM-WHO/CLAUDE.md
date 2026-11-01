# 工程约定

## 变更记录（每次推送必须更新）

主仓库是 `E:\MyProjects\DeskFriend`（remote `origin`），变更日志在仓库根目录的
`GIT_CHANGELOG.md`。

- **每次向主仓库推送（push）前，先更新 `GIT_CHANGELOG.md`。** 不只是本模块，
  任何推送到主仓库的改动都要记录。
- 在 `# Git Change Log` 标题之后按时间倒序新增一节，格式与现有条目一致：
  `## YYYY-MM-DD - \`<短哈希>\` \`<提交主题>\``，正文写明类型、范围和关键变化。
- 一次推送包含多个提交时，每个提交各写一节；合并或整理型提交要说明实际包含的
  功能/修复，不能只写分支名。
- 变更日志与代码一起提交（例如 `docs: update changelog for <短哈希>`），不要出现
  只推送代码、不更新记录的情况。

## 仓库安全

主仓库是公开仓库，以下内容不得提交：

- Wi-Fi 凭据：`include/wifi_secrets.h`（已忽略，仓库里只保留
  `wifi_secrets.example.h` 模板）。
- 含人脸的截图与抓拍：`docs/screenshots/`（整目录已忽略）。
- 未脱敏的串口日志：提交前把 SSID 替换为 `<ssid>`。

## 常用命令

```bash
# 修改网页后重新生成固件内置页面
python tools/gen_html.py

# 编译（内存紧张时限制并行任务数）
pio run -j 2

# 烧录
pio run -t upload
```
