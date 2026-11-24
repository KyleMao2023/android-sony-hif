# Android Sony HEIF Suite

[简体中文](README.zh-CN.md)

Enable **Sony Creators' App Wi-Fi HEIF transfer** and **system-level Sony `.HIF` viewing & sharing** (Gallery, WeChat, Creators' App) on rooted Android devices, without replacing stock apps or altering original camera files.

## Tested Environment

- **Device**: Xiaomi 14 Pro (`ARM64`)
- **OS**: Android 16 / HyperOS 3.0.308
- **Root / Hook**: KernelSU + Zygisk Next + LSPosed
- **Target Apps**: Xiaomi Gallery, WeChat, Sony Creators' App `3.5.0`

## Components

| Component | Type | What It Does | Docs |
| --- | --- | --- | --- |
| **Sony HEIF Entry** (`0.1.5`) | Zygisk Module | Hooks `libhwui.so` `createHeifDecoder()` in Gallery, WeChat, and Creators' App; software-decodes Sony `heix + SHIF` `.HIF` (10-bit 4:2:0 / 4:2:2 / 4:2:2 HLG) and delegates standard phone HEIC back to the system decoder. | [English](sony-heif-module/README.md) · [中文](sony-heif-module/README.zh-CN.md) |
| **Creators HEIF Wi-Fi** (`0.1.0`) | LSPosed Module | Unlocks manual Wi-Fi HEIF selection and original-file transfer in Sony Creators' App `3.5.0` without altering global settings. | [English](creators-heif-lsposed/README.md) · [中文](creators-heif-lsposed/README.zh-CN.md) |

## Quick Start (Prebuilt Releases)

1. Flash `sony-heif-entry-0.1.5-arm64.zip` in Magisk or KernelSU (Zygisk enabled).
2. Install `creators-heif-wifi-0.1.0.apk`, enable **Creators HEIF Wi-Fi** in LSPosed, and set its scope exclusively to **Creators' App (`jp.co.sony.ips.portalapp`)**.
3. Reboot or force-stop and relaunch Gallery, WeChat, and Creators' App so the new processes load the modules.
4. Connect your Sony camera via Creators' App Wi-Fi, browse photos manually, and download `.HIF` files as original files. You can now view and share them directly in Gallery and WeChat.

## Building from Source

Both modules include PowerShell build and verification scripts (`build.ps1`, `test.ps1`, `verify-package.ps1`):

```powershell
# 1. Native Zygisk decoder module
cd sony-heif-module
.\build.ps1 -AndroidSdk "<path-to-android-sdk>" -RustToolchain "<path-to-rust-toolchain>"
.\test.ps1
.\verify-package.ps1 -AndroidSdk "<path-to-android-sdk>"

# 2. Creators' App LSPosed module
cd ..\creators-heif-lsposed
.\test.ps1 -AndroidSdk "<path-to-android-sdk>"
.\build.ps1 -AndroidSdk "<path-to-android-sdk>"
.\verify-package.ps1 -AndroidSdk "<path-to-android-sdk>"
```

See each module's README for toolchain details, architecture notes, and customization instructions.

## License

- [sony-heif-module/LICENSE](sony-heif-module/LICENSE) (MIT) · [Third-party notices](sony-heif-module/THIRD_PARTY.md)
- [creators-heif-lsposed/LICENSE](creators-heif-lsposed/LICENSE) (MIT) · [Third-party notices](creators-heif-lsposed/THIRD_PARTY.md)
