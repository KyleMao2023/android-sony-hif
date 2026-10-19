# API provenance

The module implementation and host fixtures are locally authored under the accompanying MIT LICENSE. No Sony APK, source, native library, image or account data is redistributed.

`api-stubs/de/robv/android/xposed/` contains minimal compile-time declarations for legacy Xposed APIs, checked against the official XposedBridge sources (Apache-2.0, copyright rovo89 and Tungstwenty). The host-only method-parameter state is locally authored for boundary tests. These declarations are excluded from the installable APK; LSPosed supplies the real runtime implementation. The isolated, non-installable probe JAR includes declarations solely to resolve reflection contracts without activating LSPosed.

- [IXposedHookLoadPackage](https://github.com/rovo89/XposedBridge/blob/art/app/src/main/java/de/robv/android/xposed/IXposedHookLoadPackage.java)
- [XC_MethodHook](https://github.com/rovo89/XposedBridge/blob/art/app/src/main/java/de/robv/android/xposed/XC_MethodHook.java)
- [XC_LoadPackage](https://github.com/rovo89/XposedBridge/blob/art/app/src/main/java/de/robv/android/xposed/callbacks/XC_LoadPackage.java)
- [XposedBridge notice](https://github.com/rovo89/XposedBridge/blob/art/NOTICE.txt)

Apache license: `api-stubs/LICENSE-APACHE`. Android SDK/JDK tools are external build tools, not included in APK or source delivery. Root-managed LSPosed and Sony HEIF Entry are separate runtime prerequisites.
