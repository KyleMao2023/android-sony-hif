# Sony HEIF Entry 0.1.3

0.1.3 为微信的并发读取加入**同图共享、有界排队**。保留 0.1.2 的相册回退修正，每个进程仍只保留一张 RGB16 主图。独立真机测试中，同图 8 个请求全部成功，三图混合 9 个请求全部成功；0.1.2 的同图对照仅 1/8 成功。实际微信 GUI 效果待更新后验收，详见 [0.1.3 设计与验证](docs/diagnosis-0.1.3.md)。

针对本机 Android 16 ARM64 的实验性 Zygisk 模块，由 Magisk / KernelSU 管理器安装。KernelSU 需要已启用的 Zygisk 实现；本机为 Zygisk Next 1.5.2。

模块在 `com.miui.gallery` 和 `com.tencent.mm` **主进程**中接管系统 `libhwui.so` 对 `createHeifDecoder()` 的调用。识别 Sony `heix` + `SHIF` 后软件解码；其他可复位输入以原始流对象交回系统实现。没有 `system/` 目录、系统文件写入、属性修改、SELinux 放宽或 APK 修改。

## 安装与回退

1. 在管理器中安装 `dist/sony-heif-entry-0.1.3-arm64.zip`。同名文件已复制到手机 `/sdcard/Download/`，保留旧 ZIP 便于回退。
2. 按管理器要求重新加载模块，使其作用于新的应用进程。本机 root 为临时获取，本次没有自动激活、重启或停止应用。
3. 用 `adb logcat -s SonyHeif` 检查 `PLT hook commit=1 original=...`。打开原图应出现 `Sony primary 9504x6336`；新解码记录 `backend decode job=...`，读取者记录 `new decode` / `shared decode`。
4. 在微信测试连续打开、快速切换三张样片、选图预览和发送，在相册复核放大、返回与释放。旧的失败缓存及应用私有入口可能影响观察；本次未清理应用数据或缓存。

回退：在管理器停用或卸载 Sony HEIF Entry，按其要求重新加载框架。现有进程中的函数指针保留到进程退出。紧急情况下可用 root 创建 `/data/adb/modules/sony_heif_entry/disable` 后重新加载框架；不要在进程仍使用模块时手工卸载内存映射。

**0.1.0 有 Zygisk 注册崩溃；0.1.1 有回退流 CFI 崩溃。** 对应修正在 0.1.2/0.1.3 中保留。

## 当前验证

| 检查 | 结果 |
|---|---|
| Windows 快速测试 | 9 项 Rust 测试、流/回退测试通过 |
| C++ 队列测试 | 同图共享、单图生命周期、FIFO、队首超时、单个等待者取消、失败恢复、请求/字节上限、句柄跨队列生命周期通过 |
| 真机同图并发，8 个独立解码器 | 0.1.2 为 1/8；0.1.3 为 8/8，只调用一次后端 |
| 真机三图混合，9 个独立解码器 | 9/9，只调用三次后端，全部完整扫描行哈希匹配 |
| 真正系统 AImageDecoder + 本版工厂接管 | 三张原图主尺寸均为 9504×6336，1584×1056 输出成功 |
| Zygisk 匿名回调与原始流回退探针 | 均通过，普通 HEIF 经原系统解码成功 |
| ZIP/ELF | 22 项、AArch64、16 KiB LOAD 对齐、跨 DSO CFI、Zygisk 边界无 CFI、仅四项系统依赖 |
| 实际目标应用 GUI | 用户确认 0.1.1 显示/微信发送成功、0.1.2 相册几乎正常；**0.1.3 尚待更新后验收** |

探针只影响自身进程。生产模块保留 HEIF 适配器的跨 DSO CFI，Zygisk 回调边界单独编译为非 CFI 对象。回退路径交回原始系统流，避免模块代理虚表进入系统解码器。命令行探针本身不启用 CFI，平台探针另行覆盖真实系统消费者。

原系统文件描述符 AImageDecoder 能完整解码 420，422/422 HLG 的像素解码返回 -6；因此不能仅凭 `heix` 标记解释图库的放大行为。历史记录：[初版验证](docs/validation.md)、[0.1.1](docs/diagnosis-0.1.1.md)、[0.1.2](docs/diagnosis-0.1.2.md)。

## 显示与资源限制

- 解码 9504×6336 主图，支持这三张 10-bit 4:2:0 / 4:2:2 / 4:2:2 HLG 样片。原 HIF 不变。
- HLG 使用 BT.2020 到 BT.709 转换和近似 SDR 查看曲线，不是 HDR 输出，未经 Sony 显示校准。
- 内部 RGB16，再按应用请求输出 RGB565、RGBA8888、BGRA8888 或 RGBA1010102。
- 单输入 ≤128 MiB，输出及全部编码图块总和 ≤80 MP；≤64 块，每块 slice 选择的原始 SPS 尺寸 ≤16 MP、单边 ≤8192。分配前逐块验证尺寸、格式和网格覆盖。
- 每进程只保留一张 RGB16 主图，约 345 MiB。同字节输入共享只读结果，各读取者有独立行号/输出格式。最后一个读取者释放后才启动下一张。没有跨进程共享，也不在最后一个读取者释放后缓存大图。
- 不同图片按准入顺序排队：最多 8 个不同图任务、64 个正在获取结果的请求、256 MiB 已准入任务的唯一编码输入。此字节额度不包含初始化阶段未准入的输入、解码临时内存或应用输出 Bitmap，**不是进程总内存上限**。
- 后台排队最多 15 秒，应用主线程排队最多 250 毫秒；额度耗尽或排队超时仍返回失败。等待上限不能打断已开始的后端解码。同图请求加入在途任务；某个等待者超时不取消其他读取者。
- 仅接管首个 `ftyp` 完整位于 128 字节探测区、包含 `heix` 和 `SHIF` 的输入。普通输入必须可 rewind/seek(0) 才能回退；无法复位则失败。Sony 不可复位流受支持。
- 只验证本机 Android 16 私有 ABI；安装脚本拒绝其他 Android 主版本及非 ARM64。应用自带解码器、其他进程和独立区域解码路径可能绕过本入口。

## 构建与测试

已使用 `D:\Code\Android\ndk\26.3.11579264` 和 Rust 1.95.0，无需 Android Studio/Gradle/CMake。

```powershell
.\build.ps1
.\test.ps1
.\test.ps1 -FullSamples
.\verify-package.ps1
.\package-source.ps1
```

Rust 工具链默认 `C:\Users\Lenovo\.rustup\toolchains\stable-x86_64-pc-windows-gnu`，需 `aarch64-linux-android` 目标。构建使用 `--offline`，依赖固定于 Cargo.lock，需已缓存。解码器副本及许可证见 [THIRD_PARTY.md](THIRD_PARTY.md)。样片目录可由 `SONY_HIF_SAMPLES` 指定；源码包不含私人照片、构建产物或完整故障报告。

## 不激活模块的真机复核

把 `build/arm64-v8a.so`、`dist/*-probe` 复制到 `/data/local/tmp/sony-heif-entry-test`，赋予探针执行权限。以下在该目录执行：

```sh
# 完整主图、不可复位输入、全部扫描行
./heif-probe ./arm64-v8a.so --nonseek /sdcard/sony-hif/hif422.HIF

# 真正 AImageDecoder；末尾加 --stock 可测原系统
./system-probe ./arm64-v8a.so /sdcard/sony-hif/hif422.HIF ./check.ppm

# 独立进程中的 Zygisk 回调与原始流回退
./entry-probe ./arm64-v8a.so
./fallback-probe ./arm64-v8a.so ./ordinary-heic.HIF --expect-original

# 8 个同图请求、9 个混合请求；固定校验基线仅适用于本项目三张样片
./concurrent-probe ./arm64-v8a.so --same /sdcard/sony-hif/hif420.HIF
./concurrent-probe ./arm64-v8a.so --mixed /sdcard/sony-hif/hif420.HIF /sdcard/sony-hif/hif422.HIF /sdcard/sony-hif/hif422hlg.HIF
```

`ordinary-heic.HIF` 是去除 Sony 标记并将 heix 改成 heic 的临时回退测试副本，不修改原图。`tools/Push-AdbTestFile.ps1` 通过现有本机 ADB 服务传输，只允许写入上述临时测试目录。
