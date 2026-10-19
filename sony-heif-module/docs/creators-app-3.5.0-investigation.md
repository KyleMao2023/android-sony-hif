# Creators' App 3.5.0：HEIF 导入限制调查

## 版本与证据范围

检查设备上已安装的 `jp.co.sony.ips.portalapp`，versionName `3.5.0`、versionCode `350607302`，ARM64。通过只读 ADB 取得 base、ARM64 和中文 split APK，使用本机 Android SDK 的 dexlib2/baksmali 与 aapt2 检查。没有重打包、安装、修改原 APK、清理应用数据或重启设备。

base.apk SHA-256：`B0716BF5B02B7C723D54D670D97C1C6C6DAF0519A5DED5330FC4D9F3FF443F09`。

原 APK、反汇编和原始设备记录保存在被 Git 忽略的 `creators-app-audit/`；本文件只记录分析结论。未完成连接相机的动态导入验收。

## 已确认的解码调用

- `io.flutter.embedding.engine.image.ImageDecoderDefaultImpl.decodeImage` 调用 `android.graphics.ImageDecoder.createSource(ByteBuffer)` 与 `decodeBitmap`。
- `ImageDecoderHeifApi36Impl.decodeImage` 先执行上述路径，失败时用 `BitmapFactory.decodeByteArray` 再尝试。`ImageDecoderHeifPre36Impl` 使用默认系统路径后处理方向。
- `AbstractCameraItem` 的缩略图/预览回调使用 `BitmapFactory.decodeByteArray`、`decodeStream`。相机缩略图由单独的 `getThumb` 请求取得，能显示缩略图不能证明已经读取或解码 HEIF 主图。
- APK 有 Flutter、自有通信/视频处理库等；本次跟踪到的 HEIF 显示路径依赖系统 Android 图像接口。没有证据表明这些路径自带完整 Sony HEIF 软件解码器；这不等于断言应用的所有媒体格式都使用系统解码。

设备当前安装的 Sony HEIF Entry 为 0.1.3。其 Zygisk 白名单只有 `com.miui.gallery` 与 `com.tencent.mm`，因此尚未接管 Creators' App 的系统解码调用。

## 两处直接拦截

`transfer.EnumFileFormat` 的序号为 JPEG=1、RAW=2、HEIF=3、WAV=4、MP4=5。下列 `canCopy(): boolean` 只允许序号 1、2、5，其他返回 false：

- `transfer.ptp.PtpPullItem`：Wi-Fi 浏览中的条目。
- `transfer.ptp.PtpPullFileToTransfer`：实际待传输文件。
- `transfer.ptp.PtpPushItem`：相机端选择的推送条目，首版补丁不纳入此路径。

这些方法仅判断文件格式，没有查询 Android 机型、SDK 版本或解码器能力。`transfer.mtp.grid.MtpGridViewAdapter` 根据 `canCopy()` 决定是否显示选择框，详情页和可传输数量也使用它。因此用户看到预览但没有导入复选框，与静态逻辑相符。

此外，`mtp.Copy$CopyContentRunnable.prepareDownloadContent()` 在调用 `canCopy()` 之前，先检查 `getFileFormat()==HEIF`；若满足，就设置 `isHeifSkipped`、报告 `UNSUPPORTED_CONTENT` 并返回。Wi-Fi 的 `PtpPullContainer` 也使用该 `mtp.Copy`，不能只改变 UI 判定。

USB 的 `mtp.mtpobject.MtpItem.canCopy()` 还检查相机端的 `NON_TRANSFERABLE_DATA` 状态，不应把它直接无条件改为 true。用户已选择首版仅做 Wi-Fi 手动导入。

## 已存在的传输与写入能力

- `PtpPullItem.getFilesToTransfer()` 已支持从单张 HEIF 或 RAW+HEIF 组合中取得 HEIF 文件；`PtpSimpleDifferencePullItem` 也有相应设置分支，但自动/差异导入不在首版范围内。
- `AbstractCameraItem.downloadFile()` 通过 `ICameraBrowser.downloadObject` 获取对象字节；`PtpContentsBrowser` 实现相机传输。
- `database.utility.MediaCollectionUtils.insert()` 明确将 `.HIF` 登记为 `image/heif` 并写入 MediaStore。

这些证据说明应用内部保留了 HEIF 识别、组包与入库代码，使定点解除拦截有可行性。它们还不能替代真实相机上的文件传输、完整字节与打开原图验收。

## 实现审查补充

`Copy$CopyContentRunnable$$ExternalSyntheticLambda0.run()` 将尺寸保存到局部变量，再创建下载回调和 `MtpObjectDownloader`，最后依该局部变量选择 `downloadFile()` 或 `downloadScreennail()`。只改下载器构造参数不能改变已经保存的 2M 分支，可能将预览当成原图。实现改为在该次合格条目的 Runnable 作用域中覆盖尺寸读取结果，三处保持一致，全局偏好不写入。

`PtpSimpleDifferencePullItem.getFilesToTransfer()` 也会生成普通的 `PtpPullFileToTransfer`。仅排除自动父类无法限制展开后的文件，故实现要求文件实例具有手动父条目来源。来源用弱引用和对象身份记录，不依赖应用可能重载的相等比较。两项缺陷均有先失败后通过的主机回归测试。

## 选定的补丁方向

用户确认：现有 Zygisk 模块增加 Creators' App 主进程；另做独立 LSPosed 模块，仅在 3.5.0 上解除 Wi-Fi 手动导入原始 HIF 的格式限制。保留官方 APK、应用数据和原始文件，其他格式继续调用原实现。只交付可安装产物，不自动激活、重启或操控相机。

索尼官方支持表仍将 Android 静态图像列为 JPEG、RAW，iPhone/iPad 才列有 HEIF；这里是本机实验性扩展，不是官方兼容性声明：[Sony Creators' App 支持说明](https://www.sony.com/electronics/support/articles/CCCA01000)。
