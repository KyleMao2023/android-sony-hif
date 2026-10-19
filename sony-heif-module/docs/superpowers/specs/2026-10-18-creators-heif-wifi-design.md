# Creators' App Wi-Fi HEIF 导入扩展

用户已确认采用“Zygisk 解码模块 + 独立 LSPosed 模块”，首版仅支持 Wi-Fi 手动导入原始 HIF。根因及版本证据见 [调查记录](../../creators-app-3.5.0-investigation.md)。

## 交付与边界

1. Sony HEIF Entry 0.1.4：在原相册/微信主进程白名单中增加 `jp.co.sony.ips.portalapp` 主进程；保持 0.1.3 解码器、共享队列、CFI 边界和回退语义。
2. `creators-heif-lsposed/`：独立 APK，使用 LSPosed 的 legacy Xposed API，只作用于 Creators' App 主进程。官方 Sony APK、签名、账号数据、相机协议和系统文件不改变。
3. 只匹配 Creators' App versionCode `350607302` / versionName `3.5.0`。版本、方法或回调形状不匹配时记录原因并不安装功能钩子。
4. 首版不开放 USB、相机端推送和自动/差异导入的设置。HIF 直接传输原字节，不转成 JPEG；全局导入偏好不被修改。

## Java 入口与生命周期

LSPosed 加载目标主进程后，在 Application.attach 获得 Context，以 PackageManager 验证版本。在安装前解析全部必需类、方法、枚举与回调构造函数；中途安装失败则撤销本次已安装的钩子。重复 attach 不重复安装。非目标包/进程不安装入口钩子。

功能钩子只改变下列输入：

- `PtpPullItem.canCopy()` 与 `PtpPullFileToTransfer.canCopy()`：原返回值保持；仅当实际格式名为 HEIF 时允许复制。文件级对象还必须由确切的手动 `PtpPullItem.getFilesToTransfer()` 生成，以弱引用按身份记录，排除自动导入生成的同类对象。JPEG、RAW、MP4 和未知格式保留原行为，不伪装成 JPEG/iOS，也不改变 Android 型号/版本。
- `Copy$CopyContentRunnable.prepareDownloadContent()`：仅对上述两类 Wi-Fi HEIF 条目，跳过提前拒绝分支，调用同一个原缩略图回调和后续下载流程。USB/MTP、推送条目及其他格式仍执行原方法。
- 下载原图：仅在上述 Wi-Fi HEIF 条目的下载 Runnable 内，让两种尺寸读取返回 Original，使调用者保存的尺寸、回调、下载器和实际下载分支一致；不保存或修改全局偏好。作用域按线程并支持嵌套和异常清理。3.5.0 两个确切 PTP 类的 `shouldDownloadScn()` 均返回 false，不可选尺寸分支原本就取原图。

枚举按名称识别，避免把 HEIF 伪装成其他格式。创建所需回调前发生异常则记录错误并保留原调用；已开始后使用一次性回调保护，使成功、失败或同步异常最多进入一次后续下载，不能重复启动。

## 构建与依赖

复用 `D:/Code/Android` 下的 JDK 17、Android 34 SDK、D8、aapt2、zipalign 和 apksigner，避免要求 Android Studio/Gradle。Android 16 运行设备可使用 Android 34 的公开 API 编译。Xposed API 为编译期依赖，不打包 API 实现到 APK；本地签名密钥、APK、DEX 和构建目录不入 Git。记录第三方 API 来源与许可证。

## 验收

- 主机测试：目标包/主进程/精确版本约束；手动条目与文件来源；自动父条目展开后仍拒绝；其他格式/USB/推送保持；异常回退、重复初始化、每处安装失败撤销；2M 偏好下实际选择原图入口、回调/下载器尺寸一致，以及作用域清理/隔离。
- 构建检查：APK 签名、作用域/入口元数据、最小权限和编译期 API 未打包；Zygisk 的包名选择测试、现有测试与 ZIP/ELF 检查通过。
- 独立真机：新解码模块经真实 AImageDecoder 处理三张样片，回退/注册探针通过。LSPosed 已激活及目标进程钩子成功需要实际日志验证，不能从守护进程存在推断。
- 用户激活后：Wi-Fi 浏览出现 HEIF 选择框；导入完整原图，后缀和文件大小正确，有条件时与相机卡原文件核对 SHA-256；三张原图可查看细节；JPEG/RAW/MP4 与取消/断连正常。
- 推送 Zygisk ZIP 和 LSPosed APK 到 sdcard 并核对哈希；不自动安装、激活、强停、重启或操作相机。GUI 未运行时明确标注未验收。
