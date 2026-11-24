# 微信手机 HEIF 回归：探测读取破坏有限回退窗口

2026-10-20 只读检查已安装的 Sony HEIF Entry 0.1.4，以及微信 8.0.79（versionCode 3200）主进程。模块文件 SHA-256 与交付的 0.1.4 一致。用户反馈缩略图正常，打开大图或发送手机 HEIF 时失败；相机 Sony HIF 可用。

## 根因与运行证据

微信主进程的实际日志多次连续出现：

```text
SonyHeif: platform fallback failed: original stream cannot reset
HWUI: --- Failed to create image decoder with message 'invalid input'
```

`ReplayStream` 构造时无条件从原始输入读取 128 字节。`Router::init()` 判定不是 Sony 后调用 `takeOriginalFromStart()`，要求原始对象 `rewind()` 或 `seek(0)`，都失败便直接拒绝输入，尚未调用系统解码器 `init()`，也没有进入 Sony 后端或解码队列。

Android 的 `BitmapFactory.decodeStream()` 会用前缀缓冲流包装 Java InputStream；其缓冲容量取自 `SkCodec::MinBufferedBytesNeeded()`，Skia 的公开契约为 32 字节。超过该窗口后不保证能回到起点。模块读了 128 字节，破坏了原本足以让系统识别并解码的输入状态。即便 Java 对象是 ByteArrayInputStream，经这个 API 也不同于直接 `decodeByteArray()`。

来源：[Android BitmapFactory.cpp](https://android.googlesource.com/platform/frameworks/base/+/master/libs/hwui/jni/BitmapFactory.cpp)、[Skia SkCodec.h](https://github.com/google/skia/blob/main/include/codec/SkCodec.h)、[Skia 有限前缀缓冲测试](https://skia.googlesource.com/skia/+/chrome/m75/tests/FrontBufferedStreamTest.cpp)。这是公开源码对机制的支持，设备上的失败由下述对照独立确认，并不假定小米二进制与当前上游逐字相同。

## 相同样片、不同入口对照

从设备只读复制两张手机 HEIF 到被忽略的本地审计目录。两者 `ftyp` 均为 24 字节，品牌为 `heic`、`mif1`、`heic`，没有 `heix` / `SHIF`。分类为非 Sony 是正确的，失败发生在恢复原流这一步。

独立 app_process 运行相同 Java BitmapFactory 调用，先不加载模块，再只在探针自身进程中安装与模块相同的工厂接管；没有注入、停止或更改真实微信进程。两张样片结果一致，inSampleSize=4；输出分别为 768×1024、1024×768。

| 调用 | 原系统 | 0.1.4 接管 |
|---|---|---|
| decodeFile | 成功 | 成功 |
| decodeByteArray | 成功 | 成功 |
| decodeStream(FileInputStream) | 成功 | 返回 null，尺寸 -1×-1 |
| decodeStream(ByteArrayInputStream) | 成功 | 返回 null，尺寸 -1×-1 |

更早的真实 AImageDecoder 文件描述符对照也成功，主图尺寸 3072×4096。旧 system-probe 的退出码为 1 是因为它硬编码要求 Sony 的 9504×6336，实际 create/decode status 都为 0；因此该次结果以状态字段解释。新的 Java 诊断探针按调用逐项打印成功/失败，进程退出码不代表所有项成功。

## 为什么应用表现不同

- Sony HIF 被模块识别后在内部重放前缀并软件解码，不走这个普通 HEIF 回退分支。
- 相册也被接管，但文件/字节数组等可回退输入仍正常。没有跟踪相册每次调用的具体入口，不能仅凭相册正常断言它一定走某一个 API。
- QQ 不在原生模块白名单，其正常行为与未受此回退逻辑影响一致。
- 缩略图可以来自独立预览或缓存；缩略图正常不能证明打开原文件的大图/发送转换路径正常。

这些日志与单请求对照确定了本次失败发生在解码初始化前，不是 Sony 并发队列额度或 60MP 像素分配导致。它不排除其他场景另有资源问题。

0.1.3 与 0.1.4 的 router.cpp、stream.cpp 没有差异；0.1.4 的原生变更只扩展目标进程。因此这是此前为避免普通回退流 CFI 崩溃而采用的“必须归还原始对象”策略留下的兼容缺口，不是 Creators' App 的 Java 钩子干扰微信。

## 修正方向（本轮未修改生产代码）

保留交回系统原始流对象的 CFI 约束，同时避免对可排除 Sony 的普通输入读取超出可回退窗口的字节。可以分阶段探测，对已完整识别的普通 HEIC 尽早回退，再处理 Sony 候选。

不能仅把固定 128 改成 32：本项目 Sony 样片的 ftyp 为 40 字节，SHIF 位于后部；这样会漏识别相机 HIF。后续设计还需覆盖较长或不完整 ftyp、非 Sony heix 和有限回退流，确保普通文件不会因探测被破坏。

新增的回归验收应包含：有限前缀窗口的主机流测试，以及真实 BitmapFactory 的上述四种入口。原有测试只覆盖始终能回退或始终不能回退的流，没有覆盖“少读可回退，多读后不能回退”的状态变化。

证据与诊断源码位于本地忽略目录 `.work/wechat-phone-heif-20261020/`，照片位于 `heic-android-audit/wechat-phone-heif-20261020/`。本轮仅在已有临时测试目录添加并运行独立探针；未安装、停用或重载模块，未重启或强停应用，未发送消息，未修改原始照片或其他已有工作区更改。
