# Android Sony HEIF decoder

通过系统 HEIF 入口为 Android 上的 Sony HIF 原图提供软件解码的实验性 Zygisk 模块，面向 Android 16 ARM64。

当前基线 **0.1.3**：支持样片中的 10-bit 4:2:0、4:2:2、4:2:2 HLG，同图共享解码结果，不同图片有界排队。用户已确认相册和微信的实际浏览/发送测试基本通过；这不代表所有 Android 设备或应用路径都已覆盖。

实验性扩展：Sony HEIF Entry **0.1.4** 增加 Creators' App 系统解码入口；配套 [Creators HEIF Wi-Fi](creators-heif-lsposed/README.md) 为独立 LSPosed 模块，定点解除 3.5.0 的手动 Wi-Fi HEIF 导入限制。实际相机 GUI 验收单独记录。

- [模块说明、限制、安装与回退](sony-heif-module/README.md)
- [构建脚本](sony-heif-module/build.ps1)、[测试脚本](sony-heif-module/test.ps1)
- [0.1.3 设计与验证](sony-heif-module/docs/diagnosis-0.1.3.md)
- [0.1.4 Creators' App 扩展与验收记录](sony-heif-module/docs/diagnosis-0.1.4.md)
- [第三方来源与许可](sony-heif-module/THIRD_PARTY.md)、[项目许可](sony-heif-module/LICENSE)

```powershell
cd sony-heif-module
.\build.ps1
.\test.ps1
.\verify-package.ps1
```

现有脚本默认使用本机 `D:\Code\Android` 下的 NDK 和 GNU Rust 工具链；迁移机器时需按脚本参数配置。完整样片测试需要自行提供样片，参见模块说明。

Git 跟踪源码、许可证、构建工具和分析文档。相机照片、设备原始日志/故障报告、APK/反编译输出和构建目录保留在本地并被忽略；文档引用的 `docs/evidence*` 是本地验收记录，未纳入仓库。

本地 `.git` 已初始化为 `main`。若当前受限执行环境不能写入 `.git`，可在普通 PowerShell 中运行 `./Initialize-Repository.ps1`，它会检查待提交路径、创建首次提交及 `v0.1.3` 标签；已有提交时只显示状态。此脚本不改全局 Git 配置，不设置远程或推送。
