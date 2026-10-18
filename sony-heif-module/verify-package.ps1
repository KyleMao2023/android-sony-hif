param([string]$AndroidSdk='D:\Code\Android')
$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$versionLine=Get-Content -LiteralPath (Join-Path $projectRoot 'module\module.prop') | Where-Object {$_ -match '^version='}
$version=($versionLine -replace '^version=','').Trim()
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid module version'}
$zip=Join-Path $projectRoot ('dist\sony-heif-entry-'+$version+'-arm64.zip')
$moduleSo=Join-Path $projectRoot 'build\arm64-v8a.so'
$llvm=Join-Path $AndroidSdk 'ndk\26.3.11579264\toolchains\llvm\prebuilt\windows-x86_64\bin'
Add-Type -AssemblyName System.IO.Compression
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
    $names=@($archive.Entries | ForEach-Object FullName)
    if(@($names | Sort-Object -Unique).Count -ne $names.Count){throw 'Duplicate ZIP entry'}
    foreach($name in $names){
        if($name -match '\.\.|\\' -or $name -notmatch '^(module\.prop|customize\.sh|skip_mount|zygisk/arm64-v8a\.so|licenses/[^/]+(/[^/]+)?)$'){
            throw "Unexpected package entry: $name"
        }
    }
    foreach($required in @('module.prop','customize.sh','skip_mount','zygisk/arm64-v8a.so','licenses/THIRD_PARTY.md','licenses/LICENSE-ZYGISK.txt','licenses/heic-rs-0.1.4/LICENSE-APACHE','licenses/heic-rs-0.1.4/LICENSE-MIT')){
        if($required -notin $names){throw "Missing package entry: $required"}
    }
    $stream=$archive.GetEntry('zygisk/arm64-v8a.so').Open()
    $sha=[Security.Cryptography.SHA256]::Create()
    try{$packedHash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}
    if($packedHash -ne (Get-FileHash -LiteralPath $moduleSo -Algorithm SHA256).Hash){throw 'Packaged binary differs from build output'}
    $reader=[IO.StreamReader]::new($archive.GetEntry('customize.sh').Open())
    try{$installer=$reader.ReadToEnd()}finally{$reader.Dispose()}
    if($installer.Contains("`r") -or -not $installer.StartsWith('#!/system/bin/sh')){throw 'Installer must use Unix newlines and a shell shebang'}
}finally{$archive.Dispose()}
$elf=(& (Join-Path $llvm 'llvm-readelf.exe') -h -l -d $moduleSo) -join "`n"
if($LASTEXITCODE -ne 0){throw 'ELF inspection failed'}
if($elf -notmatch 'Machine:\s+AArch64'){throw 'Wrong ELF architecture'}
$loads=@($elf -split "`n" | Where-Object {$_ -match '^\s+LOAD\s'})
if(!$loads.Count -or @($loads | Where-Object {$_ -notmatch '0x4000\s*$'}).Count){throw 'ELF must have 16 KiB LOAD alignment'}
$needed=@([regex]::Matches($elf,'Shared library: \[([^\]]+)\]') | ForEach-Object {$_.Groups[1].Value} | Sort-Object)
if(($needed -join ',') -ne 'libc.so,libdl.so,liblog.so,libm.so'){throw "Unexpected runtime dependencies: $needed"}
$symbols=(& (Join-Path $llvm 'llvm-nm.exe') --dynamic --defined-only $moduleSo) -join "`n"
if($LASTEXITCODE -ne 0){throw 'Export inspection failed'}
foreach($symbol in @('zygisk_module_entry','sony_heif_create_decoder','__cfi_check')){
    if($symbols -notmatch "(?m)\s$symbol`$"){throw "Missing export: $symbol"}
}
$boundarySymbols=(& (Join-Path $llvm 'llvm-nm.exe') --undefined-only (Join-Path $projectRoot 'build\zygisk-boundary.o')) -join "`n"
if($LASTEXITCODE -ne 0 -or $boundarySymbols -match '__cfi_'){throw 'Unexpected CFI in Zygisk boundary'}
$report=[ordered]@{status='PASS';zip=$zip;zip_sha256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash;
    module_sha256=$packedHash;zip_entries=$names.Count;architecture='AArch64';load_alignment=16384;needed=$needed;entries=$names}
$json=$report | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText((Join-Path $projectRoot 'build\package-report.json'),$json,[Text.UTF8Encoding]::new($false))
$json
