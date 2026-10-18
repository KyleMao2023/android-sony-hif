# Sony HEIF system-entry module design

## Goal and authorization

Implement the system HEIF-entry adapter requested in this conversation as an uninstallable Magisk/KernelSU Zygisk module. Do not modify system files. Initial targets are the main processes of com.miui.gallery and com.tencent.mm. User supplied D:/Code/Android as the toolchain location.

The accepted in-chat approach is a Sony HEIF software backend behind the system HeifDecoder interface. Local source at D:/Code/Android-dev/heif-gallery/rust/heic-rs is available and contains the 4:2:2 mapping correction. Reuse a copy under this project, preserving upstream licenses; do not modify the source project.

## Architecture

- Zygisk module, ARM64, installs PLT interception for createHeifDecoder in already loaded libhwui.so inside the two target processes. No system overlay, property edits, service modifications, or APK patching.
- Returned router implements Android 16 HeifDecoder ABI. Sony files are selected by their ftyp compatible brand SHIF plus heix. Other streams retain the original decoder and original stream bytes.
- A bounded replay stream handles sniffing even when the input cannot rewind or report length. Sony input is read into a bounded buffer; Rust receives only the image bytes and does not need storage permissions or root.
- Pure Rust heic-rs software backend decodes the primary image, including the 3x3 Sony grid. A C ABI isolates its ownership from C++. Pixel conversion supports RGB565, RGBA8888, BGRA8888 and RGBA1010102.
- HLG is rendered to SDR through an explicitly described approximate tone map and BT.2020 to sRGB conversion. This is a viewing transform, not Sony-calibrated rendering or HDR output.
- Failure returns false, never fabricates a preview as a full-resolution image. Encoded input <=128 MiB, output and total coded tile pixels <=80 megapixels, <=64 tiles, each <=16 MP and <=8192 per dimension. Every tile is preflighted against its actual slice-selected SPS. One process-wide Rust reservation covers decoding and the entire retained RGB16 lifetime.
- Since 0.1.3, a C++ queue shares immutable decoded pixels between independent decoders with byte-identical input. Identity uses length/hash plus full-byte equality. Each reader retains its own scanline cursor and output format. The last reader frees Rust pixels before advancing the FIFO for a different image. Ready results are not cached after all readers leave; a temporary bridge preserves a result for callers already waiting.
- Queue limits: 8 unique jobs including the active one, 64 acquire calls, and 256 MiB of admitted unique encoded inputs. These do not cap initialization buffers, codec scratch memory, or application Bitmaps. Background queue wait is 15 seconds; Android main-thread queue wait is 250 ms. Timeout affects only that waiter, not other readers or already-running backend work. Capacity/timeout failures remain possible. Each target process has an independent queue.
- Code and dependencies stay mapped after hooks are installed. Unrelated processes unload the Zygisk module. No hooks in system_server.

## Deliverables

Source, vendored decoder licenses, PowerShell build script using the existing NDK and Rust toolchain, installable module ZIP, command-line Android probe, README with installation/disable/uninstall instructions, and test evidence. Module archive contains no system/ directory.

## Verification

1. Host tests: file brand routing, replay/ownership, unsupported formats, truncated and oversized input, sample metadata, primary decode and output layout.
2. ARM64 probe: decode /sdcard/sony-hif/hif420.HIF, hif422.HIF and hif422hlg.HIF through the adapter and verify 9504x6336, row traversal and color packing; truncated input must fail cleanly.
3. Static module checks: Zygisk entry export, dependencies, architecture, archive contents, no scripts writing system paths.
4. End-to-end target application checks require module activation and fresh application processes. Report this separately from standalone probe success. Do not reboot the device automatically because root is temporary.

## Known coverage limits

The first version covers the loaded system libhwui factory. App-private decoders, late-loaded libraries, format filters, caches and separate region-decode paths may bypass it. Successful standalone primary decode does not prove WeChat/gallery zoom works; label any untested path explicitly.
