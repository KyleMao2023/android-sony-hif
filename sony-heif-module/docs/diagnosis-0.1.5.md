# 0.1.5：手机 HEIF 的 InputStream 回退修正

2026-10-20。修复用户报告的“微信缩略图正常，手机 HEIF 大图/发送失败，Sony HIF 正常”。根因及旧版真实调用对照见 [诊断记录](diagnosis-wechat-phone-heif-20261020.md)。本版仅更新原生模块；Creators HEIF Wi-Fi 0.1.0 APK 无需更新。

## 修改

`ReplayStream` 原先固定读取 128 字节再区分 Sony 与普通 HEIF；普通输入回退要求归还系统自己的原始流对象。手机样片的 ftyp 仅 24 字节，额外读取破坏了 BitmapFactory InputStream 的有限前缀回退能力，导致初始化前返回失败。

现在先读 8 字节盒头，只有首盒为 ftyp、声明长度 16–128 字节且按品牌字段对齐时，才继续读至该盒末尾。短读循环补足，截断时保留实际读取内容。无效、过长或非 ftyp 盒头不再触发额外探测。24/32 字节普通头可在平台回退窗口内恢复原始对象；Sony 样片继续读全 40 字节，包含后部 SHIF。

Rust 解码器、内存上限、共享队列、颜色变换、目标包名、CFI 边界和 `takeOriginalFromStart()` 的原始对象所有权契约均未改变。版本为 0.1.5 / versionCode 6。

## 验证

1. **先失败、后通过。** 新增 32 字节有限回退流的 Router 测试，旧版无法 init；新增 40 字节 Sony 探测边界测试，旧版读取超出该盒。修正后完整测试通过。
2. **主机测试。** 9 项 Rust 快速测试、流/回退/队列/目标进程 4 组 C++ 测试、LSPosed 5 个主机测试入口通过。包括仅 rewind/仅 seek 的有限窗口、1/5 字节短读、24/32 字节普通头、非 Sony heix、未知盒、无效/超大/截断长度、原始对象身份、完整字节和一次析构。40/128 字节 Sony 头与不可复位重放通过。
3. **真实 Android API。** 两张手机 HEIF 在独立 app_process 中，以同一测试分别调用原系统、0.1.4、0.1.5。旧版两条 InputStream 路径失败且测试 exit 1；新版四种入口全部通过，exit 0。主机额外逐入口比较原系统与新版的尺寸和像素哈希，8 项比较全部一致。

| 输入 | 输出（inSampleSize=4） | ARGB 像素 FNV，原系统与 0.1.5 各四种入口均相同 |
|---|---|---|
| 手机样片 1 | 768×1024 | `54b4a8e420b1a676` |
| 手机样片 2 | 1024×768 | `b0d865de94625d56` |

四种入口是 `decodeFile`、`decodeByteArray`、`decodeStream(FileInputStream)`、`decodeStream(ByteArrayInputStream)`。公开的可重复测试源码在 `tests/android/`；`tools/Build-StreamProbe.ps1` 使用本地 JDK17、Android34 SDK、D8 和 NDK 构建 `build/android-stream-probe/{stream-probe.jar,stream-bridge.so}`。探针只接管自身进程的工厂入口，失败或跨入口像素不一致时 exit 1；不进入安装 ZIP。

4. **Sony 回归。** 三张不可复位 HIF 完整解码至 9504×6336，读取全部扫描行；哈希与既有基线一致：420 `1e746690d0587735`、422 `063810d9c54dbc1a`、422 HLG `444c433937951481`。真实 AImageDecoder 三张均成功输出 1584×1056。Sony 420 的四条 BitmapFactory 路径也均成功，2376×1584 / ARGB FNV `99f975c6bf901b59`。
5. **边界与安装包。** 匿名 Zygisk 入口探针通过；真实平台 CFI 回退探针确认收到确切原始流对象，init 成功。ZIP/ELF 验证通过：22 条目、AArch64、16 KiB 对齐、4 个原有系统依赖，边界对象无 CFI runtime 调用。独立代码审查没有 Critical/Important 问题。审查建议的原系统跨进程像素对照已执行；额外长头有限窗口负例测试暂未增加，限制如下。

## 保留限制与用户验收

这次修复覆盖本机手机样片及 24/32 字节普通 ftyp 的有限回退流。**普通 ftyp 为 33–128 字节、输入仅支持 32 字节回退时，仍可能失败。** HeifStream 没有非破坏性 peek；本版不通过自定义代理交给系统来绕过该限制，以免重新引入已诊断的 CFI 崩溃。长普通头在可正常 rewind/seek 的输入上仍受支持。未将本版宣称为任意 HEIF 输入流的通用修复。

本轮未安装、激活、重载模块，未重启、强停应用或发送消息。独立进程验证了修正机制；真实微信浏览/发送仍需用户更新后验收。临时 root 状态及原始照片保持。

安装 `/sdcard/Download/sony-heif-entry-0.1.5-arm64.zip`，按现有管理器流程使新模块进入应用进程，再验证两张手机照片和 Sony HIF 的大图/发送。LSPosed APK、作用域和 Creators' App 设置保持原状。可保留 0.1.4 ZIP 供回退。

## 产物

- ZIP SHA-256：`BAE60C830694CB8445BFB10024E413B2D04016A64494F2AF26B01210FEF816DC`。
- SO SHA-256：`3A14DA037EAD338B68CC5A4124F51901EBD2B84FA57C20A8AA952E4D1B76CF10`。
- 原始证据与任务账本留在被忽略的 `.work/heif-entry-0.1.5/`；照片不进入源码归档。
- 基于用户已提交的 `612f9ff`；本执行环境 `.git` 只读，更改作为待提交工作区交付。保留用户原有的 `.metals/metals.lock.db` 删除状态。
