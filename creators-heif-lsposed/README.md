# Creators HEIF Wi-Fi (`0.1.0`)

[简体中文](README.zh-CN.md)

A standalone LSPosed module that enables **manual Wi-Fi HEIF (`.HIF`) selection and original-file transfer** in **Sony Creators' App `3.5.0`** (`jp.co.sony.ips.portalapp`, versionCode `350607302`), while preserving the official Sony APK signature, account data, and camera firmware.

Works alongside [Sony HEIF Entry](../sony-heif-module/README.md) (`0.1.5` recommended) so transferred `.HIF` files can also be decoded and displayed inside Android Gallery, Creators' App, and WeChat.

## Tested Environment

- **Device**: Xiaomi 14 Pro (`ARM64`)
- **OS**: Android 16 / HyperOS 3.0.308
- **Framework**: KernelSU + Zygisk Next + LSPosed
- **Target App**: Sony Creators' App `3.5.0` (`350607302`)

## What It Modifies at Runtime

- Allows manual browse `PtpPullItem` entries with `HEIF` format to be selected and transferred; verifies `PtpPullFileToTransfer` instances by weak-reference origin from `getFilesToTransfer()`.
- Keeps automatic / difference imports, USB transfers, and camera-initiated push behavior unchanged.
- Routes manual Wi-Fi HEIF thumbnails through the stock callback pipeline instead of early `UNSUPPORTED_CONTENT` rejection, guarded by a one-shot callback wrapper (`OnceImageCallback`).
- Forces transfer size to **Original** strictly within the thread-local scope of that single manual HEIF download runnable—leaving JPEG/RAW/MP4 and global `2M` / `Original` settings untouched.
- Requires zero extra permissions, performs no device/iOS spoofing, and automatically disables hooks if target signatures do not match.

## Installation & Rollback

1. Install and activate **Sony HEIF Entry** (`0.1.5` Zygisk module) so Android can render Sony `.HIF` thumbnails and photos.
2. Install `creators-heif-wifi-0.1.0.apk`.
3. Open **LSPosed Manager**, enable **Creators HEIF Wi-Fi**, and select **only** `Sony Creators' App` (`jp.co.sony.ips.portalapp`) in the module scope.
4. Force-stop and relaunch Creators' App. Connect to your camera over Wi-Fi, browse photos manually, select `.HIF` files, and import them as originals.
5. Check LSPosed logs for `CreatorsHeif: installed 7 hooks for 3.5.0 manual Wi-Fi HEIF`.

**Rollback**: Uncheck **Creators HEIF Wi-Fi** in LSPosed (or uninstall the APK) and relaunch Creators' App.

## Modifying & Building from Source

### Prerequisites

- **JDK 17** (`javac`, `java`, `jar`, `keytool`)
- **Android SDK Platform 34** (`platforms/android-34/android.jar`)
- **Android SDK Build-Tools 34.0.0** (`aapt2`, `zipalign`, `apksigner`) and `cmdline-tools/lib/r8.jar` (D8)
- No Gradle installation or network dependency download is required.

### Build & Test Commands

```powershell
.\test.ps1 -AndroidSdk "<path-to-android-sdk>"
.\build.ps1 -AndroidSdk "<path-to-android-sdk>"
.\verify-package.ps1 -AndroidSdk "<path-to-android-sdk>"
```

Outputs are written to `dist/creators-heif-wifi-0.1.0.apk` and `dist/creators-contract-probe.jar`.  
Note: Local builds generate a keystore under `build/local-signing.jks` (gitignored); keep that keystore if you want to upgrade your locally signed builds in-place without uninstalling first.

## License

MIT License — see [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md).
