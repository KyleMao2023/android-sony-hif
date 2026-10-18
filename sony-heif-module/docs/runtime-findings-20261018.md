# 0.1.1 大图浏览运行记录

2026-10-18 15:53 起只读检查。用户确认系统相册、微信均能显示三张原图，微信可转为 JPG 发送。当前安装 0.1.1，模块 SHA-256 为 `8559AFA490EC5B9A9D567EB542F45DCD66E85B4796BB020E14E10CA8741564B5`。本轮未修改代码、安装文件或框架状态。

## 并发请求失败：已确认

模块日志快照中有 16 条 `decode failed: another decoded image is still live`，其中 8 条来自当前微信主进程 PID 18159（15:52:29—15:52:30）。相册旧进程 PID 25343 也有同样记录。

rust/src/lib.rs 的进程内 AtomicBool reservation 从 probe/decode 前取得，直到 Decoded 中 RGB16 缓冲释放才解除。native/router.cpp 在最后一行读取/跳过、重新 init 或解码器销毁时释放它。新解码请求在资源占用时立即失败，没有排队，也没有保证应用会重试。因此这些记录足以确认部分“图片加载失败”来自当前并发准入策略。该限制按进程生效，相册与微信并不共享一个名额。

## 相册原生崩溃：定位到系统回退路径

最新六份 tombstone 头部均来自 com.miui.gallery，tile-provider-0 线程，SIGSEGV/SEGV_ACCERR。代表事件 15:47:32：

```text
libdl.so __cfi_slowpath+28
libheif.so android::HeifDataSource::readAt
...
libheif.so HeifDecoderImpl::reinit / init
sony_heif_entry/arm64-v8a.so +0xe8308
libhwui.so SkHeifCodec::MakeFromStream
...
HeifUtil.releaseMemoryHeap
TileBitProviderForHeif.release
```

0xe8308 符号化为 Router::init。该堆栈进入原系统解码器的回退路径，CFI 读取时崩溃；不是单凭大图尺寸推测出的内存不足。应用退出历史也将最近事件记录为 Native crash/status 11 或 Signaled/status 11。具体的 CFI 映射/流对象生命周期原因还需单独复现，不能仅凭此堆栈确定为某一种内存管理错误。

## 内存：成本很高，但尚未证实为本次崩溃原因

9504×6336 = 60,217,344 像素。根据当前表示计算：

| 缓冲 | 大小 |
|---|---|
| 完整 RGB16（三通道，每通道 16-bit） | 344.57 MiB |
| 应用完整 RGBA8888 | 229.71 MiB |
| 九块 3200×2112 4:2:2 YCbCr16 | 232.03 MiB |
| 同样九块 4:2:0 YCbCr16 | 174.02 MiB |

以上会在部分阶段重叠；还需编码文件、HEVC 工作区、行转换、应用缓存等。相册和微信各有进程内名额，系统总量也可能叠加。接近/超过 1 GiB 的峰值具有可能性，但本轮未做解码期间的峰值 RSS 采样，不能当作测量结果。

采样时系统 MemAvailable 约 6.39 GiB；微信静态 RSS 1,186,264 KiB、PSS 875,229 KiB，相册 RSS 448,356 KiB。该快照不能说明之前失败瞬间的可用内存。当前日志范围内未找到与所述相册崩溃同时间的 OOM/LMKD 证据。09:26 存在一条更早的相册 LOW_MEMORY 退出，不能归于这次 0.1.1 浏览问题。

## 后续修正顺序

1. 为系统回退路径补充与实际 Zygisk 映射环境相关的流/CFI 测试，优先解决相册原生崩溃。
2. 将每进程直接拒绝的并发策略改为可取消的有界排队或相同图像任务合并；需要防止解码器持有资源等待扫描行而造成死锁，也要考虑 UI/任务超时。
3. 降低整幅 RGB16 和全部图块同时驻留的成本。现有 HeifDecoder 入口不知道应用最终缩略图目标尺寸，单纯调应用 inSampleSize 并不能保证降低本模块内部的全图成本；需要逐图块/逐行输出，或把上层采样目标传入后端。

按目标显示尺寸采样的一般原则参考 [Android 大图加载官方说明](https://developer.android.com/topic/performance/graphics/load-bitmap)。这轮只做证据收集与分类，不放开并发上限。

原始证据位于 diagnostics/ 下的 sonyheif-runtime-20261018.txt、latest-tombstone-heads-20261018.txt、crash-runtime-20261018.txt、exit-and-memory-20261018.txt、kill-allocation-20261018.txt。
