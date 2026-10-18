# 0.1.0 Zygisk 注册失败与 0.1.1 修正

## 结论

0.1.0 的构建把 Zygisk 入口也纳入跨 DSO CFI 检查。入口调用框架提供的 `registerModule` 函数指针时，检查失败并触发 SIGTRAP，发生在目标应用筛选和 HEIF 解码之前。故障源于本模块的编译边界设置；此前的解码探针没有调用 Zygisk 注册入口，因此没有覆盖该缺陷。

## 报告证据

用户提供的 `ZygiskNext-bugreport-20261018-122424.tar.gz` 读取时已不在设备目录中。实际读取同目录现存的 `ZygiskNext-bugreport-20261018-135157.tar.gz`，原始压缩包和本地提取内容保存在 `diagnostics/20261018-122424`，设备报告没有被修改。

basic.txt 包含：

```text
modules64:2,zygisk_lsposed,sony_heif_entry
modules_with_issue:1,sony_heif_entry=3@6d61696e
description=[Stop zygisk due to crash, 1 module(s) with issue: sony_heif_entry, ...]
```

tombstone_30（13:48:07），进程名称仍为 zygote64，调用链位于 forkSystemServer：

```text
signal 5 (SIGTRAP), code 1 (TRAP_BRKPT)
#00 linker64 CFIShadowWriter::CfiFail
#01 linker64 __loader_cfi_fail
#02 libdl.so __cfi_slowpath
#03 sony_heif_entry/zygisk/arm64-v8a.so zygisk_module_entry.cfi+72
#04 zygisksu/lib64/libzygisk.so
...
libandroid_runtime.so Zygote_nativeForkSystemServer
```

原模块反汇编：

```text
e7b20: ldr x21, [x20, #0x8]  // api_table.registerModule
...
e7b38: bl __cfi_slowpath
...
e7b48: blr x21              // 实际回调尚未执行
```

连续记录和 Zygisk Next 的停止保护解释了其他 Zygisk 模块同时受影响。目标包名筛选在注册后的 preAppSpecialize 才执行，不能保护此前的注册阶段。

设备安装文件 SHA-256 为 `3FA090AAB75278F476443F6F8E595B94177BE5415BF04352EA2C20E49C7EA589`，与交付的 0.1.0 一致。当前存在 `/data/adb/modules/sony_heif_entry/disable`。本次没有改动这个安装目录或其他模块。

## 复现与修正

新增 entry-probe，在独立普通 native 进程中提供模拟 Zygisk API v4 表，调用实际模块的 `zygisk_module_entry`，验证 registerModule、onLoad、preServerSpecialize 和 setOption。它不连接、不加载、不重启实际 Zygisk 框架。

| 测试 | 0.1.0 | 0.1.1 |
|---|---|---|
| 链接器已登记的普通 API 回调 | PASS / exit 0 | PASS / exit 0 |
| 匿名可执行地址中的 API 回调 | SIGTRAP / exit 133 | PASS / exit 0 |

匿名回调模拟系统链接器没有对应 CFI 目标元数据的边界，复现了报告中的同一 CFI 失败链路；没有对 Zygisk Next 内部加载实现作完整审计。CFI 间接调用检查及跨 DSO 限制参考 [Clang 官方说明](https://clang.llvm.org/docs/ControlFlowIntegrity.html)。

修正仅改变构建：module.cpp 编译为普通非 LTO、非 CFI 的目标文件；router.cpp 和 stream.cpp 继续启用跨 DSO CFI。未修改官方 Zygisk 头文件，也没有关闭系统的 CFI、修改 SELinux 或系统属性。构建和包检查额外验证入口目标文件不存在 `__cfi_*` 运行时调用。官方接口包装函数随入口在同一目标文件中编译，覆盖后续 Zygisk API 回调边界。

## 0.1.1 验证

- 模块 SHA-256：`8559AFA490EC5B9A9D567EB542F45DCD66E85B4796BB020E14E10CA8741564B5`。
- 安装 ZIP SHA-256：`5EF0AB878D39E7C6736D76D8CDFBEC1189DA11973CA51437AABB00252FD8F708`。
- 两种入口回调模式均通过，9 项快速 Rust 测试与 C++ stream 测试通过。
- 同一二进制经真实系统 AImageDecoder + 平台 CFI 再次解码三张 HIF：create=0、decode=0，主图 9504×6336，校验输出 1584×1056。
- 包验证：22 文件、AArch64、16 KiB LOAD 对齐、保留 __cfi_check 导出、仅四项平台动态依赖。
- 原始测试结果见 `docs/evidence-0.1.1`。

**尚未验证真实框架激活及微信/相册 GUI。** 手机原 0.1.0 保持禁用；0.1.1 只放在 `/data/local/tmp/sony-heif-entry-test` 运行探针，没有安装、启用、重启、清缓存或操作其他模块。Zygisk Next 先前触发的停止保护，也没有在本次诊断中重置。
