# Sony HEIF module implementation plan

> Implement inline with superpowers:executing-plans; use meaningful red/green tests and preserve validation evidence.

**Goal:** Build an uninstallable ARM64 Zygisk module that routes Sony HIF through a software backend at the system HEIF entry.

**Architecture:** PLT hook in target app processes; Android HeifDecoder router; C ABI Rust decoder copied from the local HEIF project. Native probe exercises the same adapter.

**Tech Stack:** NDK 26.3, C++17, Rust stable GNU host and aarch64-linux-android, Zygisk API, PowerShell.

**Spec:** docs/design.md

## Global constraints

- No modification of system files or the source HEIF Gallery project.
- Targets com.miui.gallery and com.tencent.mm main processes only.
- Input limit 128 MiB, output/total coded pixel limit 80 MP, single live software decoded image per process.
- No automatic device reboot or claims of untested GUI success.

## Review focus

Non-seekable stream replay; original decoder ownership; ABI and CFI compatibility; concurrent decode memory; malformed or unsupported Sony files.

## Task 1: Reproducible offline decoder and C ABI

- [x] Copy local decoder source/licenses and locate cached Rust toolchain.
- [x] Add failing Rust tests for three samples' metadata, primary decode, invalid input and HLG color conversion.
- [x] Implement probe/decode/free C ABI with bounded inputs and panic containment.
- [x] Run host tests and build staticlib for Android.

## Task 2: Android HeifDecoder adapter

- [x] Write C++ replay/brand-routing tests and Rust pixel-format tests; observe failure before implementation.
- [x] Implement router, ownership rules, original-path delegation and lifetime reservation.
- [x] Build standalone ARM64 probe and test three originals through the actual interface.

## Task 3: Zygisk packaging and checks

- [x] Vendor official Zygisk API header with provenance and implement process allowlist plus libhwui PLT hook.
- [x] Package module.prop, installer checks, zygisk/arm64-v8a.so and licenses; no system/ tree.
- [x] Verify exports, dependencies, package contents and recovery instructions.
- [x] Record target-app runtime validation separately; do not reboot automatically.

Pending live activation: Zygisk loading and Gallery/WeChat GUI behavior are explicitly unverified. No installation/reboot is part of the completed probe validation.
