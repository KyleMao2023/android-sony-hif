# Creators' App Wi-Fi HEIF Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Enable original-HIF manual Wi-Fi import in Creators' App 3.5.0 while retaining the official APK and the existing Gallery/WeChat decoder behavior.

**Architecture:** Sony HEIF Entry 0.1.4 adds one target process. A separate legacy LSPosed APK performs exact-version, PTP-only format checks and uses the application's existing download continuation. Neither artifact is activated automatically.

**Tech Stack:** C++17/NDK, existing Rust backend, Java 8 bytecode built with JDK17, Android34 SDK, D8/aapt2/apksigner, PowerShell.

**Spec:** `../specs/2026-10-18-creators-heif-wifi-design.md` (user approved in this conversation).

## Global Constraints

- Main process `jp.co.sony.ips.portalapp` only; Java hooks require versionName `3.5.0` and versionCode `350607302`.
- Manual Wi-Fi PTP HEIF only; official APK/signature, original bytes, global settings and USB/push/automatic settings remain unchanged.
- Preserve existing native queue, CFI boundary, fallback and resource limits.
- No automatic installation/activation, application stop, reboot or camera operation.
- API compile stubs and private APK/decompilation/key files must not enter the runtime APK or source publication.

## Review Focus

1. Other formats and PTP subclasses used for automatic import must retain original results.
2. A mismatched app version or missing callback must install no feature hooks.
3. A partial hook installation must unhook its predecessors.
4. Duplicate callback delivery or a synchronous thumbnail exception must not start two downloads.
5. Per-item Original size must not alter the global image-size preference.

## Task 1: Extend the native decoder allowlist

**Files:** `sony-heif-module/native/target_process.h`, `native/module.cpp`, `tests/target_process_test.cpp`, `test.ps1`, `module/module.prop`, README.

**Interfaces:** `bool isTargetProcess(const char* name)` accepts exactly the two existing main processes and the Creators' App main process, rejecting null and subprocess names.

- [x] Add and run the process-selection test; observe Creators' App failure.
- [x] Implement the exact allowlist and call it from module.cpp; set version0.1.4/code5.
- [x] Run `test.ps1`; build and verify the native ZIP. Do not change the Rust backend.
- [x] Validate the final binary with existing entry, fallback and real-system three-sample probes.

## Task 2: Implement the scoped Java hook behavior

**Files:** `creators-heif-lsposed/src/local/sonyheif/creators/{HookPolicy,OnceImageCallback,CreatorsHooks,Entry}.java`; `tests/` and compile-only `api-stubs/`.

**Interfaces:**

- `HookPolicy.applies(String packageName, String processName, String versionName, long versionCode)`.
- `HookPolicy.allowHeif(String concreteClassName, String formatName)`: only the exact `PtpPullItem` and `PtpPullFileToTransfer` types with HEIF are eligible.
- `OnceImageCallback`: an invocation handler forwarding at most one success/failure completion to the original callback.
- `CreatorsHooks.install(ClassLoader)`: resolves all required methods/fields/constructor/Original enum before registering hooks; returns the installed handles and rolls back on registration failure.
- `Entry.handleLoadPackage`: targets only the main process, validates the installed version after Application.attach, and installs once.

- [x] Write failing tests for package/process/version filtering, exact eligible types/formats and non-HEIF preservation.
- [x] Implement HookPolicy, then test success/failure/duplicate callback combinations for OnceImageCallback, including concurrent delivery.
- [x] Test reflection bindings and hook rollback with authored fixtures matching the APK's verified method shapes; label these as host contracts, not ART integration.
- [x] Implement the two `canCopy` hooks, original callback continuation for HEIF prepareDownloadContent, and scoped Original size reads before the caller stores its transfer decision (see review correction in diagnosis-0.1.4.md).
- [x] Handle pre-start exceptions by leaving the original method; after takeover use a guarded failure completion. Leave unsupported inputs and thrown original results unchanged.
- [x] Implement exact-version Application.attach lifecycle; log disabled reasons without paths, account or camera identifiers.

## Task 3: Build, inspect and deliver the LSPosed APK

**Files:** `creators-heif-lsposed/{AndroidManifest.xml,assets/xposed_init,res/values/scope.xml,build.ps1,test.ps1,README.md,THIRD_PARTY.md,LICENSE}`; project README and investigation/release notes.

**Interfaces:** `build.ps1 -AndroidSdk D:/Code/Android` produces `dist/creators-heif-wifi-0.1.0.apk`; `test.ps1` runs host Java contracts. The APK contains own feature classes only, no `de.robv.android.xposed` class definitions.

- [x] Build compile-only API declarations separately; compile own sources against Android34/API signatures.
- [x] Use D8/aapt2, add only feature DEX/assets/resources, zipalign and sign with a retained local test key excluded from Git.
- [x] Verify signature, manifest scope/version, absence of permissions/services and absence of API implementations/proprietary files in the APK.
- [x] Run host tests and a focused independent code review. Correct Critical/Important findings and rerun affected checks.
- [x] Push the native ZIP and LSPosed APK to `/sdcard/Download/` through the existing temporary ADB staging directory; compare hashes.
- [x] Update documentation with actual checks and explicit untested GUI/camera activation. Preserve baseline/tag and avoid committing via alternate Git metadata paths; if `.git` remains protected, report the prepared source changes for the user's ordinary Git session.

## User acceptance after activation

Enable only the Creators' App scope in LSPosed, load the new native module, and use a fresh app process according to the managers' workflow. Connect to the camera, verify HEIF checkboxes, original file size/extension and pixel detail, JPEG/RAW behavior, cancellation and disconnection. Compare original-file SHA-256 when a reference file is available. A standalone probe does not establish these GUI results.
