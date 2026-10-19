# Creators HEIF Wi-Fi 0.1.0

独立 LSPosed 模块，与 Sony HEIF Entry 0.1.4 配合，为 **Creators' App 3.5.0（350607302）主进程**开放手动 Wi-Fi 导入原始 HIF。保留官方 Sony APK、签名、账号数据和相机固件。

## 使用与回退

1. 用现有 Magisk/KernelSU 管理器更新 `sony-heif-entry-0.1.4-arm64.zip`。
2. 安装 `dist/creators-heif-wifi-0.1.0.apk`，在 LSPosed 中启用 **Creators HEIF Wi-Fi**，作用域仅选择 **Creators' App / jp.co.sony.ips.portalapp**。
3. 按已有管理器流程使模块进入新的应用进程。本次不自动安装、激活、重载、重启或停止应用。
4. 在 Creators' App 中通过 Wi-Fi 浏览相机照片，选择 HEIF，手动导入。补丁对该次 HEIF 下载指定 Original，不修改全局 2M/原图偏好，不转换为 JPG。
5. 检查选择框、下载完成、文件后缀/大小和放大细节；可用相机卡原文件核对 SHA-256。JPEG/RAW/MP4、取消和断连也须回归。

LSPosed 日志应出现 `CreatorsHeif: installed 7 hooks for 3.5.0 manual Wi-Fi HEIF`。原生 `SonyHeif` 日志应有工厂接管记录及打开主图时的 `Sony primary` / `new decode` / `shared decode`。模块可见、守护进程存在或独立探针成功不能单独证明目标应用的钩子已激活。

回退：在 LSPosed 停用本模块，按其流程让目标应用进入新进程。原生模块可以单独回退至 0.1.3，见 [原生模块说明](../sony-heif-module/README.md)。

## 修改范围

- 手动浏览的确切 `PtpPullItem` 类在格式为 HEIF 时可选择；`PtpPullFileToTransfer` 还必须由手动条目的 `getFilesToTransfer()` 生成，以弱引用按对象身份记录来源。自动/差异导入生成的同类文件对象保持拒绝；USB 和相机端推送保持。
- 手动 Wi-Fi HEIF 使用应用原有缩略图回调及后续下载逻辑，绕过提前 `UNSUPPORTED_CONTENT` 的分支。
- 成功、失败、同步异常与重复通知通过一次性回调保护，避免重复继续下载。
- 仅在该次 HEIF 下载 Runnable 的线程作用域中，让尺寸读取返回 Original，使回调、下载器和原图/预览分支一致。作用域支持嵌套、异常清理和线程隔离；不写全局设置。3.5.0 两个确切 PTP 类的 `shouldDownloadScn()` 均固定为 false，不可选尺寸的分支原本就选原图。
- 其他版本、缺失接口或初始化失败时关闭功能钩子；安装中途失败撤销已有钩子；入口与功能安装各只尝试一次。
- 没有机型/SDK/iOS 伪装、系统全局替换、联网/存储权限或后台服务。

本模块解除客户端格式限制。原生解码仍有 60MP 内存成本和排队上限，HLG 仍为近似 SDR 查看。实际连接相机的 GUI 验收单独记录，见 [0.1.4 发布记录](../sony-heif-module/docs/diagnosis-0.1.4.md)。

## 构建与验证

```powershell
.\test.ps1
.\build.ps1
.\verify-package.ps1
```

默认复用 `D:\Code\Android` 的 JDK17、Android34 SDK、D8、aapt2、zipalign、apksigner，不需 Gradle 或下载依赖。`api-stubs/` 为编译期声明和主机测试参数容器，**不进入可安装 APK**。主机测试使用自行编写的夹具，不包含 Sony 实现，也不冒充 ART 集成验收。[来源与许可](THIRD_PARTY.md)。

本地测试签名密钥在被 Git 忽略的 `build/local-signing.jks`；保留它才能继续签名升级本模块。删除后重新生成的密钥无法覆盖旧签名安装。源码仓库不含密钥；正式发布应使用自己维护的发布密钥。

`dist/creators-contract-probe.jar` 是不安装钩子的独立接口探针，含仅供探针使用的 API 声明。它检查真实 Sony APK 的类/方法/字段/回调构造函数，不能替代实际 LSPosed 或相机导入测试。
