# Android Sony HEIF Suite（安卓索尼 HEIF 套件）

[English](README.md)

在已 Root 的 Android 设备上打通 **Sony Creators' App Wi-Fi 导入 `.HIF` 原图** 与 **系统级索尼 `.HIF` 看图/微信发送** 全链路，不修改系统分区或官方应用安装包，也不改动相机原始文件。

## 测试环境

- **设备**：Xiaomi 14 Pro（`ARM64`）
- **系统**：Android 16 / HyperOS 3.0.308
- **Root / 框架**：KernelSU + Zygisk Next + LSPosed
- **实测应用**：小米相册、微信、Sony Creators' App `3.5.0`

## 包含模块

| 模块 | 类型 | 功能说明 | 说明文档 |
| --- | --- | --- | --- |
| **Sony HEIF Entry**（`0.1.5`） | Zygisk 模块 | 在相册、微信、Creators' App 主进程中接管 `libhwui.so` 的 `createHeifDecoder()`；对 Sony `heix + SHIF` `.HIF`（10-bit 4:2:0 / 4:2:2 / 4:2:2 HLG）进行软件解码，普通手机 HEIF/HEIC 交回原系统解码。 | [English](sony-heif-module/README.md) · [中文](sony-heif-module/README.zh-CN.md) |
| **Creators HEIF Wi-Fi**（`0.1.0`） | LSPosed 模块 | 解除 Sony Creators' App `3.5.0` 通过 Wi-Fi 手动浏览/导入相机 `.HIF` 原图的格式限制，不影响全局传输设置与其他格式。 | [English](creators-heif-lsposed/README.md) · [中文](creators-heif-lsposed/README.zh-CN.md) |

## 快速安装（使用 Release 预编译包）

1. 在 Magisk 或 KernelSU（需开启 Zygisk）中刷入 `sony-heif-entry-0.1.5-arm64.zip`。
2. 安装 `creators-heif-wifi-0.1.0.apk`，在 LSPosed 中启用 **Creators HEIF Wi-Fi**，并将作用域仅勾选 **Creators' App (`jp.co.sony.ips.portalapp`)**。
3. 重启或强行停止并重新打开相册、微信及 Creators' App。
4. 在 Creators' App 中通过 Wi-Fi 手动浏览相机照片并导入 `.HIF` 原图，随后即可在系统相册、微信等应用中直接预览与发送。

## 从源码编译与修改

两个子项目均附带了 PowerShell 测试、编译与校验脚本（`test.ps1`、`build.ps1`、`verify-package.ps1`）：

```powershell
# 1. 编译 Zygisk 原生解码模块
cd sony-heif-module
.\build.ps1 -AndroidSdk "<Android SDK 路径>" -RustToolchain "<Rust 工具链路径>"
.\test.ps1
.\verify-package.ps1 -AndroidSdk "<Android SDK 路径>"

# 2. 编译 Creators' App LSPosed 模块
cd ..\creators-heif-lsposed
.\test.ps1 -AndroidSdk "<Android SDK 路径>"
.\build.ps1 -AndroidSdk "<Android SDK 路径>"
.\verify-package.ps1 -AndroidSdk "<Android SDK 路径>"
```

各模块的源码结构、限制说明及自定义参数调整请分别查看子目录 README。

## 开源许可

- [sony-heif-module/LICENSE](sony-heif-module/LICENSE)（MIT） · [第三方来源与声明](sony-heif-module/THIRD_PARTY.md)
- [creators-heif-lsposed/LICENSE](creators-heif-lsposed/LICENSE)（MIT） · [第三方来源与声明](creators-heif-lsposed/THIRD_PARTY.md)
