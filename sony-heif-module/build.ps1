param(
    [string]$AndroidSdk='D:\Code\Android',
    [string]$RustToolchain='C:\Users\Lenovo\.rustup\toolchains\stable-x86_64-pc-windows-gnu',
    [switch]$SkipRust
)
$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$ndk=Join-Path $AndroidSdk 'ndk\26.3.11579264'
$llvm=Join-Path $ndk 'toolchains\llvm\prebuilt\windows-x86_64'
$clang=Join-Path $llvm 'bin\clang++.exe'
$native=Join-Path $projectRoot 'native'
$build=Join-Path $projectRoot 'build'
$dist=Join-Path $projectRoot 'dist'
$versionLine=Get-Content -LiteralPath (Join-Path $projectRoot 'module\module.prop') | Where-Object {$_ -match '^version='}
$version=($versionLine -replace '^version=','').Trim()
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid module version'}
foreach($dir in @($build,$dist)){[IO.Directory]::CreateDirectory($dir)|Out-Null}
if(-not $SkipRust){
    $priorRustc=$env:RUSTC
    try{
        $env:RUSTC=Join-Path $RustToolchain 'bin\rustc.exe'
        & (Join-Path $RustToolchain 'bin\cargo.exe') build --offline --release --target aarch64-linux-android --lib --manifest-path (Join-Path $projectRoot 'rust\Cargo.toml') --config (Join-Path $projectRoot 'rust\.cargo\config.toml')
        if($LASTEXITCODE -ne 0){throw 'Rust Android build failed'}
    }finally{$env:RUSTC=$priorRustc}
}
$archive=Join-Path $projectRoot 'rust\target\aarch64-linux-android\release\libsony_hif_codec.a'
$common=@('--target=aarch64-linux-android26',"--sysroot=$llvm\sysroot",'-std=c++17','-O2','-fPIC','-ffunction-sections','-fdata-sections',
    '-fvisibility=hidden','-fvisibility-inlines-hidden','-flto','-fsanitize=cfi','-fsanitize-cfi-cross-dso','-static-libstdc++',
    '-Wall','-Wextra','-Werror',"-I$native",'-Wl,--gc-sections','-Wl,--exclude-libs,ALL','-Wl,-z,relro','-Wl,-z,now','-Wl,-z,max-page-size=16384')
$moduleSo=Join-Path $build 'arm64-v8a.so'
# Zygisk API callbacks may live outside bionic's CFI metadata. Keep this
# boundary in a normal non-LTO object; only the HEIF adapter uses cross-DSO CFI.
$boundaryArgs=@($common | Where-Object {$_ -ne '-flto' -and $_ -ne '-static-libstdc++' -and $_ -notlike '-fsanitize*' -and $_ -notlike '-Wl,*'})
$boundaryObject=Join-Path $build 'zygisk-boundary.o'
& $clang @boundaryArgs '-fno-lto' '-fno-sanitize=cfi' '-fno-sanitize-cfi-cross-dso' '-c' (Join-Path $native 'module.cpp') '-o' $boundaryObject
if($LASTEXITCODE -ne 0){throw 'Zygisk boundary build failed'}
$boundarySymbols=(& (Join-Path $llvm 'bin\llvm-nm.exe') --undefined-only $boundaryObject) -join "`n"
if($LASTEXITCODE -ne 0 -or $boundarySymbols -match '__cfi_'){throw 'Zygisk boundary must not call CFI runtime'}
& $clang @common -shared $boundaryObject (Join-Path $native 'router.cpp') (Join-Path $native 'stream.cpp') (Join-Path $native 'decode_queue.cpp') $archive '-llog' '-ldl' '-lm' "-Wl,--version-script=$native\exports.map" '-o' $moduleSo
if($LASTEXITCODE -ne 0){throw 'Zygisk native build failed'}
& $clang @common '-fno-sanitize=cfi' '-fno-sanitize-cfi-cross-dso' '-fPIE' '-pie' (Join-Path $native 'probe.cpp') '-ldl' '-o' (Join-Path $build 'heif-probe')
if($LASTEXITCODE -ne 0){throw 'Android probe build failed'}
& $clang @common '--target=aarch64-linux-android30' '-fno-sanitize=cfi' '-fno-sanitize-cfi-cross-dso' '-fPIE' '-pie' (Join-Path $native 'system_probe.cpp') '-ldl' '-o' (Join-Path $build 'system-probe')
if($LASTEXITCODE -ne 0){throw 'System-entry probe build failed'}
& $clang @common '-fno-sanitize=cfi' '-fno-sanitize-cfi-cross-dso' '-fPIE' '-pie' (Join-Path $native 'entry_probe.cpp') '-ldl' '-o' (Join-Path $build 'entry-probe')
if($LASTEXITCODE -ne 0){throw 'Zygisk entry probe build failed'}
& $clang @common '-fno-sanitize=cfi' '-fno-sanitize-cfi-cross-dso' '-fPIE' '-pie' (Join-Path $native 'fallback_probe.cpp') '-ldl' '-o' (Join-Path $build 'fallback-probe')
if($LASTEXITCODE -ne 0){throw 'Fallback probe build failed'}
& $clang @common '-fno-sanitize=cfi' '-fno-sanitize-cfi-cross-dso' '-fPIE' '-pie' (Join-Path $native 'concurrent_probe.cpp') '-ldl' '-o' (Join-Path $build 'concurrent-probe')
if($LASTEXITCODE -ne 0){throw 'Concurrent probe build failed'}
$package=Join-Path $build ('package-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory((Join-Path $package 'zygisk'))|Out-Null
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'module') -File | Copy-Item -Destination $package -Force
Copy-Item -LiteralPath $moduleSo -Destination (Join-Path $package 'zygisk\arm64-v8a.so') -Force
$licenses=Join-Path $package 'licenses'
[IO.Directory]::CreateDirectory($licenses)|Out-Null
Get-ChildItem -LiteralPath $projectRoot -Filter 'LICENSE*' -File | Copy-Item -Destination $licenses -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY.md') -Destination $licenses -Force
$priorRustc=$env:RUSTC
try{
    $env:RUSTC=Join-Path $RustToolchain 'bin\rustc.exe'
    $metadata=& (Join-Path $RustToolchain 'bin\cargo.exe') metadata --offline --format-version 1 --manifest-path (Join-Path $projectRoot 'rust\Cargo.toml') | ConvertFrom-Json
    if($LASTEXITCODE -ne 0){throw 'License metadata failed'}
}finally{$env:RUSTC=$priorRustc}
foreach($p in $metadata.packages){
    if($p.name -eq 'sony_hif_codec'){continue}
    $licenseDir=Join-Path $licenses ($p.name+'-'+$p.version)
    [IO.Directory]::CreateDirectory($licenseDir)|Out-Null
    Get-ChildItem -LiteralPath (Split-Path -Parent $p.manifest_path) -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE)' } | Copy-Item -Destination $licenseDir -Force
}
$zip=Join-Path $dist ('sony-heif-entry-'+$version+'-arm64.zip')
if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip}
Add-Type -AssemblyName System.IO.Compression
$zipStream=[IO.File]::Open($zip,[IO.FileMode]::CreateNew)
$zipArchive=[IO.Compression.ZipArchive]::new($zipStream,[IO.Compression.ZipArchiveMode]::Create)
try{
    foreach($file in Get-ChildItem -LiteralPath $package -Recurse -File){
        $entryName=[IO.Path]::GetRelativePath($package,$file.FullName).Replace('\','/')
        $entry=$zipArchive.CreateEntry($entryName,[IO.Compression.CompressionLevel]::Optimal)
        $entryStream=$entry.Open();$input=[IO.File]::OpenRead($file.FullName)
        try{$input.CopyTo($entryStream)}finally{$input.Dispose();$entryStream.Dispose()}
    }
}finally{$zipArchive.Dispose();$zipStream.Dispose()}
Copy-Item -LiteralPath (Join-Path $build 'heif-probe') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $build 'system-probe') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $build 'entry-probe') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $build 'fallback-probe') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $build 'concurrent-probe') -Destination $dist -Force
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | Format-List Path,Hash
