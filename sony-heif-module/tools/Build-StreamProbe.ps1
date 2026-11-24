param([string]$AndroidSdk='D:\Code\Android')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build\android-stream-probe'
$work=Join-Path $out ([Guid]::NewGuid().ToString('N'))
foreach($dir in @($out,"$work/classes","$work/dex")){[IO.Directory]::CreateDirectory($dir)|Out-Null}
$bin=Join-Path $AndroidSdk 'jdk-17.0.20.1+1\bin'
$android=Join-Path $AndroidSdk 'platforms\android-34\android.jar'
function Check-Exit([string]$step){if($LASTEXITCODE -ne 0){throw "Stream probe build failed: $step"}}
& "$bin/javac.exe" --release 8 -cp $android -d "$work/classes" (Join-Path $root 'tests/android/StreamProbe.java')
Check-Exit 'javac'
& "$bin/jar.exe" --create --file "$work/classes.jar" -C "$work/classes" .
Check-Exit 'classes jar'
& "$bin/java.exe" -cp (Join-Path $AndroidSdk 'cmdline-tools/lib/r8.jar') com.android.tools.r8.D8 --min-api 33 --lib $android --output "$work/dex" "$work/classes.jar"
Check-Exit 'D8'
& "$bin/jar.exe" --create --file "$out/stream-probe.jar" -C "$work/dex" classes.dex
Check-Exit 'dex jar'
$clang=Join-Path $AndroidSdk 'ndk/26.3.11579264/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe'
& $clang --target=aarch64-linux-android30 -std=c++17 -O2 -fPIC -shared -static-libstdc++ -I (Join-Path $root 'native') (Join-Path $root 'tests/android/stream_bridge.cpp') -ldl '-Wl,-z,max-page-size=16384' -o "$out/stream-bridge.so"
Check-Exit 'JNI bridge'
Get-FileHash "$out/stream-probe.jar","$out/stream-bridge.so" | Format-List Path,Hash
