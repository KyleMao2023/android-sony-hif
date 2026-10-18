# Third-party provenance

- heic-rs 0.1.4: https://github.com/tbraun96/heic-rs, MIT OR Apache-2.0. Copied from D:/Code/Android-dev/heif-gallery/rust/heic-rs, including the existing corrected HEVC Table 8-3 MAP_422. Source and licenses are included under rust/vendor/heic-rs. This module adds `hevc::preflight_still` in `src/hevc/mod.rs` to expose the exact slice-selected SPS geometry before picture allocations. Its acceptance tests live in `rust/src/lib.rs`. The original source project remains unchanged.
- Zygisk API v4: official header from https://github.com/topjohnwu/zygisk-module-sample at commit 7bb941ac8edfcffd1d23761e401c45ca95409dc1, file module/jni/zygisk.hpp, Git blob 7c861adcdafa738cbb03bb708b7f132f34eef013. The header's permissive license is retained verbatim.
- HeifDecoder ABI declarations: Copyright (C) 2017 The Android Open Source Project, frameworks/av/media/libheif/include/HeifDecoderAPI.h, Apache-2.0; adapted declarations matching the Android 16 interface with getColorDepth(). Apache license is bundled alongside the decoder.
- Cargo dependencies: rayon, rayon-core, crossbeam-utils, crossbeam-epoch, crossbeam-deque and either. Cargo.lock fixes the versions; their license files are bundled in the module ZIP.

The module targets interoperability and does not include Sony or Tencent proprietary code, binaries or images. The standalone probe and module use the same newly built adapter.
