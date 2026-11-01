# 打包桌面客户端为单文件 exe（Windows）。
# 用法：在 usb_client 目录执行  powershell -ExecutionPolicy Bypass -File build_exe.ps1
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$python = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
if (-not (Test-Path $python)) {
    python -m venv .venv
    $python = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
    & $python -m pip install -r requirements.txt
}

& $python -m pip install --disable-pip-version-check -q pyinstaller
& $python -m PyInstaller --noconfirm --clean -F -w -n DeskFriend-Camera app.py

$exe = Join-Path $PSScriptRoot "dist\DeskFriend-Camera.exe"
if (Test-Path $exe) {
    $size = [math]::Round((Get-Item $exe).Length / 1MB)
    Write-Host "OK: $exe ($size MB)"
} else {
    throw "打包失败，产物不存在：$exe"
}
