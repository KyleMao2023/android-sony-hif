param([string]$AndroidSdk='D:\Code\Android')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$apk=Join-Path $root 'dist\creators-heif-wifi-0.1.0.apk'
$java=Join-Path $AndroidSdk 'jdk-17.0.20.1+1\bin\java.exe'
$bt=Join-Path $AndroidSdk 'build-tools\34.0.0'
$signature=& $java -jar "$bt/lib/apksigner.jar" verify --verbose --print-certs $apk
if($LASTEXITCODE -ne 0){throw 'APK signature check failed'}
$signature | Set-Content (Join-Path $root 'build\apk-signature.txt')
& "$bt/zipalign.exe" -c 4 $apk
if($LASTEXITCODE -ne 0){throw 'APK alignment check failed'}
$manifest=(& "$bt/aapt2.exe" dump xmltree $apk --file AndroidManifest.xml) -join "`n"
if($LASTEXITCODE -ne 0){throw 'Manifest inspection failed'}
if($manifest -match 'E: (uses-permission|service|receiver|provider|activity)\b'){throw 'Unexpected application component/permission'}
foreach($value in @('local.sonyheif.creators.module','xposedmodule','xposedminversion','xposedscope')){if(-not $manifest.Contains($value)){throw "Missing metadata: $value"}}
$resources=(& "$bt/aapt2.exe" dump resources $apk) -join "`n"
if($LASTEXITCODE -ne 0 -or $resources -notmatch 'array/xposed_scope' -or $resources -notmatch 'jp.co.sony.ips.portalapp'){throw 'Missing target scope'}
Add-Type -AssemblyName System.IO.Compression
$archive=[IO.Compression.ZipFile]::OpenRead($apk)
$classes=[Collections.Generic.List[string]]::new()
try{
    $reader=[IO.StreamReader]::new($archive.GetEntry('assets/xposed_init').Open())
    try{if($reader.ReadToEnd().Trim() -ne 'local.sonyheif.creators.Entry'){throw 'Invalid module entry'}}finally{$reader.Dispose()}
    foreach($entry in $archive.Entries){
        if($entry.FullName -notmatch '^(AndroidManifest\.xml|resources\.arsc|classes[0-9]*\.dex|assets/xposed_init|assets/licenses/[^/]+|res/[^/]+/[^/]+)$'){throw "Unexpected APK entry: $($entry.FullName)"}
        if($entry.FullName -notmatch '^classes[0-9]*\.dex$'){continue}
        $stream=$entry.Open();$memory=[IO.MemoryStream]::new()
        try{$stream.CopyTo($memory);$bytes=$memory.ToArray()}finally{$stream.Dispose();$memory.Dispose()}
        function U32([int]$offset){return [BitConverter]::ToUInt32($bytes,$offset)}
        $count=U32 96;$classOffset=U32 100;$typeOffset=U32 68;$stringOffset=U32 60
        for($i=0;$i -lt $count;$i++){
            $typeIndex=U32 ($classOffset+32*$i)
            $stringIndex=U32 ($typeOffset+4*$typeIndex)
            $offset=U32 ($stringOffset+4*$stringIndex)
            do{$b=$bytes[$offset];$offset++}while($b -band 128)
            $start=$offset;while($bytes[$offset] -ne 0){$offset++}
            $name=[Text.Encoding]::UTF8.GetString($bytes,$start,$offset-$start)
            if($name -notmatch '^Llocal/sonyheif/creators/'){throw "Foreign/API/test class packaged: $name"}
            $classes.Add($name)
        }
    }
}finally{$archive.Dispose()}
if('Llocal/sonyheif/creators/Entry;' -notin $classes -or 'Llocal/sonyheif/creators/CreatorsHooks;' -notin $classes){throw 'Feature classes missing'}
$report=[ordered]@{status='PASS';apk=$apk;sha256=(Get-FileHash $apk -Algorithm SHA256).Hash;class_count=$classes.Count;class_definitions=$classes;scope='jp.co.sony.ips.portalapp';signature_verified=$true;api_implementations_packaged=$false;permissions=@()}
$report | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $root 'build\package-report.json')
"APK verification PASS: $($classes.Count) own class definitions, valid signature, exact scope, no permissions"
