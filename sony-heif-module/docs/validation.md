# 验证记录（2026-10-18）

**本文件记录 0.1.0 的历史探针结果。后续实际安装暴露了该版本 Zygisk 入口的 CFI 崩溃，0.1.0 应保持禁用。0.1.1 修正及最新结果见 diagnosis-0.1.1.md。**

交付物为 Android 16 ARM64 的实验性 Zygisk 模块；已验证系统解码器适配路径，尚未验证 Zygisk 实际激活及微信/相册 GUI。

## 二进制与环境

- ZIP SHA-256：`48B39854CBD52F3EDEEA6581614D510783F50B2AC9AAAC745A9F1E54DF8515DF`
- `zygisk/arm64-v8a.so` SHA-256：`3FA090AAB75278F476443F6F8E595B94177BE5415BF04352EA2C20E49C7EA589`
- ZIP 中、本机构建目录中、手机临时目录中的模块二进制 SHA-256 相同。
- 手机：小米 23116PN5BC / SM8650，Android 16 API 36，HyperOS OS3.0.308.0.WNBCNXM。
- 现有框架：KernelSU 32680，Zygisk Next 1.5.2，LSPosed 2.2.1；本模块使用 Zygisk API v4，不依赖 LSPosed。
- 构建：NDK 26.3.11579264，Rust 1.95.0，Android aarch64。

## 测试结果

`.\test.ps1 -FullSamples`：9 项快速 Rust 测试通过，1 项显式完整主图测试通过，C++ stream 测试通过。覆盖像素格式打包、HLG 查看曲线、主图/网格元数据、最后一块损坏的 NAL、网格尺寸矛盾、实际 SPS 过大、累计图块预算、保留解码图像时拒绝新请求及释放后恢复。完整测试依次解码三张 9504×6336 RGB16 主图，约 9.55 秒。

真机同一模块二进制，不可 seek / rewind / 报告长度的输入流：

| 输入 | 输出尺寸 | RGBA8888 FNV-1a | 含扫描行转换耗时 |
|---|---|---|---|
| hif420.HIF | 9504×6336 | 1e746690d0587735 | 4160 ms |
| hif422.HIF | 9504×6336 | 063810d9c54dbc1a | 4635 ms |
| hif422hlg.HIF | 9504×6336 | 444c433937951481 | 5344 ms |

真实系统 `libjnigraphics` / `AImageDecoder` 探针修改自身进程的 `libhwui` HEIF 工厂 GOT，调用交付模块，实现了与系统消费者及 CFI 的集成验证。三张均 `create=0`、`decode=0`，返回原始主图尺寸，输出 1584×1056 校验图。探针内部仍完整解码主图，缩小由平台消费者处理。

对照：原系统通过文件描述符读 420 可以解码完整主图；422 和 422 HLG 可以读取头信息，但像素解码返回 -6。这修正了此前把品牌标记差异当作充分根因的判断。

回退路径：仅在临时副本的 `ftyp` 中把 `SHIF` 改为 `test`，强制走原解码器；该 420 副本通过真实系统消费者成功解码。保留 Sony 标记、仅有前 128 字节的截断副本返回初始化失败，探针退出码 1；没有崩溃。测试副本仅位于构建/手机临时测试目录。

`.\verify-package.ps1`：22 个 ZIP 文件条目通过白名单检查；模块为 AArch64、16 KiB LOAD 对齐；导出 `zygisk_module_entry`、`sony_heif_create_decoder`、`__cfi_check`。运行时依赖仅 `libc.so`、`libdl.so`、`liblog.so`、`libm.so`。没有 system 目录、启动服务、属性补丁或 SELinux 规则。

## 保留状态与边界

没有安装模块、重启、停止应用或修改 system 文件。新增真机文件仅在 `/data/local/tmp/sony-heif-entry-test`。三张原图 SHA-256 与最初一致，见 evidence/device-final-state.txt。

独立代码审查提出的两项内存问题已修复并复审。该审查不等于完整 HEVC 解码器安全审计；没有测量峰值 RSS。每个目标进程只允许一张仍存活的软件解码主图，约 345 MiB RGB16，另外还需图块及应用缓冲区。

尚未验证：Zygisk 装载、PLT 注册、denylist 配置、管理器安装/回退流程、微信/相册中的浏览与缩放。应用私有解码器、子进程及独立区域解码路径可能绕过此入口。HLG 为近似 SDR 查看变换，未校准为 Sony 显示，也不输出 HDR。

原始探针输出和 ZIP 检查结果保存在 [evidence](evidence)。
