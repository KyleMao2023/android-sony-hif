# 0.1.2：系统回退流与 CFI 崩溃修正

## 根因

0.1.1 将 ReplayStream 代理对象交给了原系统 HeifDecoderImpl。代理的虚表属于 Zygisk 加载的模块映射；原 libheif 在 HeifDataSource::readAt 中执行 CFI 虚调用检查时，读取了对应的不可访问 shadow 页，触发 SIGSEGV。

完整 tombstone 的证据（2026-10-18 15:47:32）：

- CFI 目标 x1：`0x2f338a2e88`，位于模块的虚表数据映射。
- shadow 基址 x9：`0x78bbe09000`，索引 x8：`0x1799c4`。
- 故障地址：`0x78bbf829c4`，等于基址加索引。
- tombstone 的故障映射：`0x78bbe0d000–0x78bc0f4000`，权限 `---`，名称 `[anon:cfi shadow]`。
- 调用链：`__cfi_slowpath → HeifDataSource::readAt → HeifDecoderImpl::init → Router::init`，图库上层为图块释放路径。

这与动态链接器的 CFI shadow 管理机制一致：shadow 初始为 PROT_NONE，加载已知库时再建立对应可读条目。参考 [AOSP linker_cfi.cpp](https://raw.githubusercontent.com/aosp-mirror/platform_bionic/master/linker/linker_cfi.cpp)。本次没有修改或修补系统 shadow。

## 修正

ReplayStream 新增 takeOriginalFromStart：对其持有的底层原始流执行 rewind 或 seek(0)，成功后转移唯一所有权。Router 回退时将这个**原始对象**交给系统，不再传入 ReplayStream 代理。因此系统继续看到自己的流类型、虚表与生命周期。

如果普通输入不能恢复到开头，返回失败并释放输入，不传入已消费的流或自定义代理。未消费字节的空流可以原样归还。Sony 路径仍在模块内部使用 ReplayStream，因此 Sony 不可回退流解码保持可用。

修改范围不包括 Rust 解码后端、图像尺寸/内存上限、HLG 曲线、并发准入策略、Zygisk 目标包名或其他模块。

## 先失败、后通过的验证

1. 新增 C++ 回退契约测试。0.1.1 在“系统收到的必须是原始对象”断言失败；修正后原始对象身份、完整字节、仅 rewind/仅 seek、无法复位、委托初始化失败和空流场景全部通过，输入只析构一次。
2. 新增真机 fallback-probe，调用真实系统 libheif；诊断 gate 对旧模块代理的虚表建立独立匿名副本，模拟缺失 CFI shadow 条目的 Zygisk 映射。探针只在自身进程中创建映射，不连接或修改真实框架。

| 探针模式 | 0.1.1 | 0.1.2 |
|---|---|---|
| 普通已登记代理/回退 | 平台 init 成功，9504×6336 | 原始流 init 成功，9504×6336 |
| 未登记代理虚表 | SIGSEGV，exit 139 | 交回原流，无需代理虚表，exit 0 |
| 连续 30 次独立平台初始化 | 未运行 | 全部通过 |

用于直接 libheif 初始化的普通回退样本是 hif420 的临时副本，ftyp 的 SHIF 改为 test、heix 改为 heic，以适配原平台容器识别；不是另一个相机样片，也没有修改原图。保留 heix、仅去除 SHIF 的副本还单独通过了 AImageDecoder 回退测试。

3. 新模块通过 Zygisk API 匿名回调入口测试，真实 AImageDecoder 三张原图测试、普通输入回退测试均成功。
4. 原生扫描行探针对三张 Sony 不可回退流全部成功，9504×6336，RGBA8888 哈希与旧版相同：420 `1e746690d0587735`、422 `063810d9c54dbc1a`、HLG `444c433937951481`。
5. 9 项快速 Rust 测试、两组 C++ 测试及 ZIP/ELF 检查通过。Rust 后端未改动；本轮完整原图验收由真机完成。

只读代码审查未发现 Critical/Important 问题。根据审查建议，探针新增 `--expect-original`，让原始对象身份同时成为退出码验收条件；旧版对照可不加该选项。它在两种回调模式下均通过。

## 交付与边界

- ZIP SHA-256：`655D6C2F08B8B487C2327DCF248B6948096A7DAEEBCBE37B301587D1CDF1C876`。
- 模块 SHA-256：`8F85804EFBE20461C13483E7F622FF5FB62B57F61CE24F61D9F874FF0CC3CAE1`，与设备独立探针使用文件一致。
- 原始结果见 docs/evidence-0.1.2。完整 tombstone 和映射留在 diagnostics/，不放入源码发布包。
- 手机安装目录仍为 0.1.1，本轮没有安装/激活/重启、停止应用、放宽 SELinux 或修改其他模块，三张原图哈希保持不变。
- **0.1.2 的真实相册浏览、缩放、返回/释放 GUI 尚待更新后验收。** 隔离模拟验证了触发机制和修正，但没有模拟完整 Zygisk 加载生命周期。
- 每进程单图准入仍在，因此并发请求导致的微信“加载失败”和高内存成本不在此版本修正范围内。
