# Creators HEIF Wi-Fi（`0.1.0`）

[English](README.md)

独立 LSPosed 模块：在保留 Sony 官方 APK、签名、账号数据与相机固件的前提下，为 **Sony Creators' App `3.5.0`**（`jp.co.sony.ips.portalapp`，`versionCode 350607302`）主进程解锁**通过 Wi-Fi 手动浏览并导入 `.HIF` 原图**。

建议搭配 [Sony HEIF Entry](../sony-heif-module/README.zh-CN.md)（推荐 `0.1.5`）使用，以便导入后的 `.HIF` 在 Creators' App 预览、系统相册与微信中均能正常显色查看。

## 测试环境

- **设备**：Xiaomi 14 Pro（`ARM64`）
- **系统**：Android 16 / HyperOS 3.0.308
- **框架**：KernelSU + Zygisk Next + LSPosed
- **目标应用**：Sony Creators' App `3.5.0`（`350607302`）

## 运行时改动范围

- 允许手动浏览界面的 `PtpPullItem` 在格式为 HEIF 时被勾选；并通过弱引用校验 `PtpPullFileToTransfer` 来源是否确实由手动条目 `getFilesToTransfer()` 生成。
- 自动导入、差异导入、USB 传输与相机端推送保持原行为不变。
- 让手动 Wi-Fi HEIF 复用官方缩略图回调与后续下载逻辑，跳过提前返回的 `UNSUPPORTED_CONTENT` 分支，并通过一次性回调守卫（`OnceImageCallback`）防止重复触发。
- 仅在该次手动 HEIF 下载 `Runnable` 的线程作用域内将尺寸临时指定为 **Original**，不修改应用全局 `2M / 原图` 偏好设置，不影响 JPEG/RAW/MP4。
- 无需额外权限、不做任何机型或 iOS 伪装；目标版本或签名不匹配时自动关闭功能钩子。

## 安装与回退

1. 建议先刷入并启用 **Sony HEIF Entry（`0.1.5` Zygisk 模块）**，确保设备可正常渲染 `.HIF` 缩略图与原图。
2. 安装 `creators-heif-wifi-0.1.0.apk`。
3. 打开 **LSPosed 管理器**，启用 **Creators HEIF Wi-Fi**，并在作用域中**仅勾选 Creators' App（`jp.co.sony.ips.portalapp`）**。
4. 强行停止并重新启动 Creators' App，通过 Wi-Fi 连接相机后手动挑图，即可选择 `.HIF` 原图导入手机。
5. 可在 LSPosed 日志中看到 `CreatorsHeif: installed 7 hooks for 3.5.0 manual Wi-Fi HEIF`。

**回退方式**：在 LSPosed 中停用本模块（或直接卸载 APK），随后重启 Creators' App 即可恢复默认行为。

## 修改与从源码编译

### 环境准备

- **JDK 17**（`javac`、`java`、`jar`、`keytool`）
- **Android SDK Platform 34**（`platforms/android-34/android.jar`）
- **Android SDK Build-Tools 34.0.0**（`aapt2`、`zipalign`、`apksigner`）及 `cmdline-tools/lib/r8.jar`（D8）
- 纯原生脚本编译，无需 Gradle 或联网拉取 Maven 依赖。

### 编译与验证命令

```powershell
.\test.ps1 -AndroidSdk "<Android SDK 路径>"
.\build.ps1 -AndroidSdk "<Android SDK 路径>"
.\verify-package.ps1 -AndroidSdk "<Android SDK 路径>"
```

编译产物位于 `dist/creators-heif-wifi-0.1.0.apk` 与 `dist/creators-contract-probe.jar`。  
提示：本地编译会在 `build/local-signing.jks` 生成自签名密钥（已加入 `.gitignore`），后续若需覆盖升级自己编译的 APK，保留该密钥即可。

## 开源许可

MIT License —— 详见 [LICENSE](LICENSE) 与 [THIRD_PARTY.md](THIRD_PARTY.md)。
