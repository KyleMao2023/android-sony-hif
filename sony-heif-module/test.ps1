param([switch]$FullSamples,[string]$RustToolchain='C:\Users\Lenovo\.rustup\toolchains\stable-x86_64-pc-windows-gnu')
$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$priorRustc=$env:RUSTC
$priorSamples=$env:SONY_HIF_SAMPLES
if(-not $env:SONY_HIF_SAMPLES){$env:SONY_HIF_SAMPLES=[IO.Path]::GetFullPath((Join-Path $projectRoot '../heic-android-audit/samples'))}
try{
    $env:RUSTC=Join-Path $RustToolchain 'bin\rustc.exe'
    $cargo=Join-Path $RustToolchain 'bin\cargo.exe'
    & $cargo test --offline --manifest-path (Join-Path $projectRoot 'rust\Cargo.toml') --lib
    if($LASTEXITCODE -ne 0){throw 'Rust fast tests failed'}
    if($FullSamples){
        & $cargo test --offline --release --manifest-path (Join-Path $projectRoot 'rust\Cargo.toml') --lib -- --ignored
        if($LASTEXITCODE -ne 0){throw 'Full-resolution sample test failed'}
    }
}finally{$env:RUSTC=$priorRustc;$env:SONY_HIF_SAMPLES=$priorSamples}
$gpp='D:\Program Files\Mingw64\bin\g++.exe'
if(-not(Test-Path -LiteralPath $gpp)){throw 'Set the host g++ path in test.ps1'}
& $gpp -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'native') (Join-Path $projectRoot 'native\stream.cpp') (Join-Path $projectRoot 'tests\stream_test.cpp') -o (Join-Path $projectRoot 'tests\stream_test.exe')
if($LASTEXITCODE -ne 0){throw 'Stream test compile failed'}
& (Join-Path $projectRoot 'tests\stream_test.exe')
if($LASTEXITCODE -ne 0){throw 'Stream tests failed'}
& $gpp -std=c++17 -pthread -Wall -Wextra -Werror -I (Join-Path $projectRoot 'native') (Join-Path $projectRoot 'native\router.cpp') (Join-Path $projectRoot 'native\stream.cpp') (Join-Path $projectRoot 'native\decode_queue.cpp') (Join-Path $projectRoot 'tests\fallback_test.cpp') -o (Join-Path $projectRoot 'tests\fallback_test.exe')
if($LASTEXITCODE -ne 0){throw 'Fallback test compile failed'}
& (Join-Path $projectRoot 'tests\fallback_test.exe')
if($LASTEXITCODE -ne 0){throw 'Fallback tests failed'}
& $gpp -std=c++17 -O2 -pthread -Wall -Wextra -Werror -I (Join-Path $projectRoot 'native') (Join-Path $projectRoot 'native\decode_queue.cpp') (Join-Path $projectRoot 'tests\decode_queue_test.cpp') -o (Join-Path $projectRoot 'tests\decode_queue_test.exe')
if($LASTEXITCODE -ne 0){throw 'Decode queue test compile failed'}
& (Join-Path $projectRoot 'tests\decode_queue_test.exe')
if($LASTEXITCODE -ne 0){throw 'Decode queue tests failed'}
& $gpp -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'native') (Join-Path $projectRoot 'tests\target_process_test.cpp') -o (Join-Path $projectRoot 'tests\target_process_test.exe')
if($LASTEXITCODE -ne 0){throw 'Target process test compile failed'}
& (Join-Path $projectRoot 'tests\target_process_test.exe')
if($LASTEXITCODE -ne 0){throw 'Target process tests failed'}
