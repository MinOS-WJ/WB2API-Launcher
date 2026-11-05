<div align="center">
  <img src="logo.svg" width="96" alt="WB2API Launcher">

  # WB2API Launcher

  **WorkBuddy2API-Hub 网关的现代化 Windows 图形启动器**

  [![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE.txt)
  [![Platform](https://img.shields.io/badge/Platform-Windows-blue.svg)](#)
  [![Win32](https://img.shields.io/badge/Win32-API-orange.svg)](#)

  纯 Win32 API 编写，零依赖、单文件、开箱即用。
</div>

---

## ✨ 功能

- **一键启动 / 重启** WorkBuddy2API-Hub 网关（`wb-proxy`）的全部脚本：
  - 🔴 本地启动（仅本机访问，推荐）
  - ⚪ 本地·可见窗口（前台控制台看日志）
  - 🔴 局域网启动（允许局域网设备访问）
  - ⚪ 局域网·可见窗口
  - ⚪ 重启服务 · ⚪ 防火墙放行
- **导入 ZIP 初始化**：手动从上游 Release 下载压缩包后，一键自动解压完成部署
- **现代化界面**：品牌红主题、卡片式按钮、DPI 高分屏适配、Segoe UI 字体
- **轻量**：单 exe 约 200KB，无运行时依赖，无需管理员权限

## 📦 安装

从 [Releases](../../releases) 下载 `WB2APILauncher-Setup.exe`，双击安装
（当前用户安装，无需 UAC，装到 `%LOCALAPPDATA%\Programs`）。

## 🚀 快速开始

1. 安装并启动 WB2API Launcher
2. 点右上角「**前往下载**」打开上游 Release 页，下载 `workbuddy2api-hub-*.zip`
3. 点「**导入 ZIP**」选中下载好的压缩包，自动解压初始化
4. 点击任意启动按钮开始使用

> 核心网关软件 **wb-proxy** 来自上游项目
> [ardeyouxipianyi/workbuddy2api-hub](https://github.com/ardeyouxipianyi/workbuddy2api-hub)，
> 本仓库只是它的图形启动器。

## 🛠️ 自行编译

需要 Visual Studio（MSVC）与 NSIS：

```powershell
# 编译资源（图标 + manifest）
rc /nologo app.rc

# 编译启动器
cl /nologo /std:c++17 /EHsc /W4 /O2 /utf-8 WB2APILauncher.cpp app.res /link /SUBSYSTEM:WINDOWS /OUT:WB2APILauncher.exe

# 打包安装程序
makensis installer.nsi
```

## 📄 许可

- **启动器（本仓库）**：MIT License，版权归 [MinOS-WJ](https://github.com/MinOS-WJ)
- **核心软件 wb-proxy**：MIT License，版权归
  [ardeyouxipianyi](https://github.com/ardeyouxipianyi/workbuddy2api-hub)

详见 [LICENSE.txt](LICENSE.txt)。
