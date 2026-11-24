# Sony HEIF Entry（`0.1.5`）

[English](README.md)

基于 Zygisk API v4+（ARM64）的系统级索尼 `.HIF`（`heix + SHIF`）软解模块：在相册、微信和 Creators' App 主进程中接管 `libhwui.so` 对 `createHeifDecoder()` 的调用，补齐 Android 端对 **10-bit 4:2:0、4:2:2、4:2:2 HLG** 索尼原图的解码与分享支持。

## 测试环境

- **设备**：Xiaomi 14 Pro（`ARM64`）
- **系统**：Android 16 / HyperOS 3.0.308
- **Root 框架**：KernelSU + Zygisk Next
- **实测应用**：小米相册（`com.miui.gallery`）、微信（`com.tencent.mm`）、Sony Creators' App（`jp.co.sony.ips.portalapp`）

## 工作原理

1. **进程内 PLT Hook**：仅在目标应用主进程中拦截 `libhwui.so` 对 `createHeifDecoder()` 的调用，不改动 `/system` 分区、系统属性、SELinux 规则或应用 APK。
2. **分级 `ftyp` 探测（`0.1.5`）**：先读取 8 字节 ISO BMFF 盒头，仅在有效 `ftyp`（16–128 字节）范围继续读取至盒尾，判断是否为 Sony `heix + SHIF`。普通手机拍摄的 HEIF/HEIC（24/32 字节头）保留在 `BitmapFactory` `InputStream` 的可回退窗口内，通过 `takeOriginalFromStart()` 将**未经包装的原系统流对象**交回系统解码器。
3. **软解与色调映射**：通过 Rust（`heic-rs`）解码最高 60MP（`9504×6336`）的索尼主图至 `RGB16`。遇到 HLG（`transfer_characteristics == 18`）时，应用 BT.2020→sRGB 色域转换与 ACES Filmic 曲线（`exposure = 4.8x`）映射至 SDR，并按上层需求输出 `RGB565`、`RGBA8888`、`BGRA8888` 或 `RGBA1010102`。
4. **并发与内存保护**：
   - 并发打开同一张图片时共享内存中的只读解码结果（单张 60MP RGB16 约 `345 MiB`，最后一个读取者释放后立即回收）。
   - 不同图片通过进程内有界 FIFO 队列排队（最多 8 张排队任务、64 个等待读取者、256 MiB 已准入压缩输入；等待上限为后台 15 秒 / 主线程 250 毫秒）。
   - Zygisk 回调边界（`native/module.cpp`）单独编译为非 LTO / 非 CFI 对象；HEIF 适配器与队列（`router.cpp`、`stream.cpp`、`decode_queue.cpp`）保留跨 DSO CFI 保护。

## 安装与回退

1. 在 Magisk 或 KernelSU（需已开启 Zygisk）中刷入 `sony-heif-entry-0.1.5-arm64.zip`。
2. 重启手机或强行停止并重新启动目标应用，使新进程加载模块。
3. 可选通过 ADB 查看运行日志：
   ```sh
   adb logcat -s SonyHeif
   ```
   正常接管后会输出 `PLT hook commit=1 original=...`，打开原图时出现 `Sony primary 9504x6336`。

**回退方式**：在 Root 管理器中停用或卸载 **Sony HEIF Entry** 并重启框架/手机；紧急情况下可用 root 创建 `/data/adb/modules/sony_heif_entry/disable` 后重启。

## 修改与从源码编译

### 环境准备

- **Android NDK**：`26.3.11579264`（需启用 16 KiB 页对齐 `-Wl,-z,max-page-size=16384`）
- **Rust**：Stable 工具链 + `aarch64-linux-android` 目标（`rustup target add aarch64-linux-android`）
- **主机 C++ 编译器（用于跑单元测试）**：MinGW-w64 `g++`（C++17）

### 编译与验证命令

1. 按需调整 [rust/.cargo/config.toml](rust/.cargo/config.toml) 中的 NDK `linker` 路径（或设置 `CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER`）。
2. 编译模块压缩包与设备探针工具：
   ```powershell
   .\build.ps1 -AndroidSdk "<Android SDK 路径>" -RustToolchain "<Rust 工具链路径>"
   ```
   构建产物输出在 `build/` 与 `dist/sony-heif-entry-0.1.5-arm64.zip`。
3. 运行主机单元测试与包结构/ELF 校验：
   ```powershell
   .\test.ps1 -RustToolchain "<Rust 工具链路径>"
   .\verify-package.ps1 -AndroidSdk "<Android SDK 路径>"
   ```

### 常见二次开发入口

- **Tone Mapping 与色彩管线**：`rust/src/lib.rs`（ACES 曲线参数与 BT.2020→sRGB 矩阵）
- **目标应用包名列表**：`native/target_process.h`
- **内存与排队阈值**：`native/decode_queue.cpp` / `rust/src/lib.rs`
- **流探测与回退边界**：`native/stream.cpp`、`native/router.cpp`

## 开源许可

MIT License —— 详见 [LICENSE](LICENSE)、[LICENSE-ZYGISK.txt](LICENSE-ZYGISK.txt) 与 [THIRD_PARTY.md](THIRD_PARTY.md)。
