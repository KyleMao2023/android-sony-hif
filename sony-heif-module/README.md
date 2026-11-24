# Sony HEIF Entry (`0.1.5`)

[简体中文](README.zh-CN.md)

A Zygisk module (API v4+, ARM64) that provides software decoding for Sony `.HIF` (`heix + SHIF`) photos—including **10-bit 4:2:0, 4:2:2, and 4:2:2 HLG**—by intercepting `libhwui.so`'s `createHeifDecoder()` PLT entry in Gallery, WeChat, and Creators' App.

## Tested Environment

- **Device**: Xiaomi 14 Pro (`ARM64`)
- **OS**: Android 16 / HyperOS 3.0.308
- **Root Framework**: KernelSU + Zygisk Next
- **Verified Apps**: Xiaomi Gallery (`com.miui.gallery`), WeChat (`com.tencent.mm`), Sony Creators' App (`jp.co.sony.ips.portalapp`)

## How It Works

1. **PLT Hook**: In the main process of target apps, hooks `libhwui.so`'s PLT call to `createHeifDecoder()`. No `/system` files, properties, SELinux rules, or target APKs are modified.
2. **Staged `ftyp` Probing (`0.1.5`)**: Reads the 8-byte ISO BMFF box header first and, when valid, reads the full `ftyp` box (16–128 bytes) to detect Sony `heix` + `SHIF`. Ordinary phone HEIF/HEIC files (24/32-byte `ftyp`) remain within Android `BitmapFactory` `InputStream`'s rewind window and are handed back using the **original stock stream object** (`takeOriginalFromStart()`) to preserve CFI safety and native hardware decoding.
3. **Decoding & Tone Mapping**: Uses Rust (`heic-rs`) to decode 60MP (`9504×6336`) Sony `.HIF` primaries into internal `RGB16`. For HLG (`transfer_characteristics == 18`), applies BT.2020→sRGB gamut conversion and an ACES Filmic tone-mapping curve (`exposure = 4.8x`) for vivid SDR viewing/sharing; outputs `RGB565`, `RGBA8888`, `BGRA8888`, or `RGBA1010102` as requested.
4. **Concurrency & Memory Guardrails**:
   - Identical concurrent image requests share a single cached decode in memory (`~345 MiB` RGB16 per active primary; evicted as soon as the last reader releases it).
   - Distinct images are serialized via a bounded per-process FIFO queue (max 8 queued images, 64 active readers, 256 MiB queued compressed inputs; wait limits: 15s background / 250ms main thread).
   - Zygisk registration boundary (`native/module.cpp`) is compiled without LTO/CFI, while the HEIF decoder adapter (`router.cpp`, `stream.cpp`, `decode_queue.cpp`) enables cross-DSO CFI.

## Installation & Rollback

1. Flash `sony-heif-entry-0.1.5-arm64.zip` in Magisk or KernelSU (Zygisk enabled).
2. Reboot or restart target apps so new processes load the module.
3. Optional verification via ADB:
   ```sh
   adb logcat -s SonyHeif
   ```
   Look for `PLT hook commit=1 original=...` and `Sony primary 9504x6336` when opening a `.HIF` file.

**Rollback**: Disable or remove **Sony HEIF Entry** in your root manager and restart the framework/device (or create `/data/adb/modules/sony_heif_entry/disable` and reboot).

## Modifying & Building from Source

### Prerequisites

- **Android NDK**: `26.3.11579264` (Clang++ `aarch64-linux-android26` / `30`, `llvm-nm`, `llvm-readelf`, 16 KiB page alignment `-Wl,-z,max-page-size=16384`)
- **Rust**: Stable toolchain with target `aarch64-linux-android` (`rustup target add aarch64-linux-android`)
- **Host C++ Compiler (for host unit tests)**: MinGW-w64 `g++` (C++17)

### Build & Test Commands

1. Update linker path in [rust/.cargo/config.toml](rust/.cargo/config.toml) if your NDK path differs, or set `CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER`.
2. Build module ZIP + Android CLI probes:
   ```powershell
   .\build.ps1 -AndroidSdk "<path-to-android-sdk>" -RustToolchain "<path-to-rust-toolchain>"
   ```
   Outputs are written to `build/` and `dist/sony-heif-entry-0.1.5-arm64.zip`.
3. Run host Rust & C++ tests and verify ELF/ZIP integrity:
   ```powershell
   .\test.ps1 -RustToolchain "<path-to-rust-toolchain>"
   .\verify-package.ps1 -AndroidSdk "<path-to-android-sdk>"
   ```

### Key Files for Customization

- **Tone Mapping / Color Pipeline**: `rust/src/lib.rs` (`hlg_to_sdr` ACES curve and BT.2020→sRGB matrix)
- **Target Apps**: `native/target_process.h` (main process names hooked on startup)
- **Memory & Queue Limits**: `native/decode_queue.cpp` / `rust/src/lib.rs`
- **Stream / `ftyp` Detection**: `native/stream.cpp` & `native/router.cpp`

## License

MIT License — see [LICENSE](LICENSE), [LICENSE-ZYGISK.txt](LICENSE-ZYGISK.txt), and [THIRD_PARTY.md](THIRD_PARTY.md).
