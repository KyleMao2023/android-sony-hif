param([string]$AndroidSdk='D:\Code\Android')
$ErrorActionPreference='Stop'
$java=Join-Path $AndroidSdk 'jdk-17.0.20.1+1\bin\java.exe'
$javac=Join-Path $AndroidSdk 'jdk-17.0.20.1+1\bin\javac.exe'
$root=$PSScriptRoot
$out=Join-Path $root 'build\host-tests'
[IO.Directory]::CreateDirectory($out)|Out-Null
$sources=@(Get-ChildItem (Join-Path $root 'src'),(Join-Path $root 'api-stubs'),(Join-Path $root 'tests') -Recurse -Filter *.java | Where-Object {$_.Name -ne 'Entry.java'})
& $javac --release 8 -Xlint:all -d $out $sources.FullName
if($LASTEXITCODE -ne 0){throw 'Host Java compile failed'}
foreach($test in @('PolicyCallbackTest','HookContractTest','InstallOnceTest','TransferOriginTest','OriginalTransferTest')){
    & $java -cp $out ('local.sonyheif.creators.'+$test)
    if($LASTEXITCODE -ne 0){throw "Host test failed: $test"}
}
