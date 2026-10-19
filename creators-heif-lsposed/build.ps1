param([string]$AndroidSdk='D:\Code\Android')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$jdk=Join-Path $AndroidSdk 'jdk-17.0.20.1+1\bin'
$java=Join-Path $jdk 'java.exe';$javac=Join-Path $jdk 'javac.exe';$jar=Join-Path $jdk 'jar.exe'
$androidJar=Join-Path $AndroidSdk 'platforms\android-34\android.jar'
$bt=Join-Path $AndroidSdk 'build-tools\34.0.0'
$d8=Join-Path $AndroidSdk 'cmdline-tools\lib\r8.jar'
$build=Join-Path $root 'build'
$dist=Join-Path $root 'dist'
$work=Join-Path $build ('apk-'+[guid]::NewGuid().ToString('N'))
foreach($dir in @($dist,$work,"$work/api","$work/classes","$work/dex","$work/probe","$work/probe-dex")){[IO.Directory]::CreateDirectory($dir)|Out-Null}
function Require-Exit([string]$step){if($LASTEXITCODE -ne 0){throw "$step failed: $LASTEXITCODE"}}
$apiSources=@(Get-ChildItem (Join-Path $root 'api-stubs') -Recurse -Filter *.java)
& $javac --release 8 -Xlint:all -d "$work/api" $apiSources.FullName;Require-Exit 'Compile API signatures'
& $jar cf "$work/api.jar" -C "$work/api" .;Require-Exit 'Archive API signatures'
$sources=@(Get-ChildItem (Join-Path $root 'src') -Recurse -Filter *.java)
& $javac --release 8 -Xlint:all -cp "$androidJar;$work/api" -d "$work/classes" $sources.FullName;Require-Exit 'Compile module'
& $jar cf "$work/program.jar" -C "$work/classes" .;Require-Exit 'Archive module classes'
& $java -cp $d8 com.android.tools.r8.D8 --release --min-api 33 --lib $androidJar --classpath "$work/api.jar" --output "$work/dex" "$work/program.jar";Require-Exit 'D8 module'
& "$bt/aapt2.exe" compile --dir (Join-Path $root 'res') -o "$work/resources.zip";Require-Exit 'Compile resources'
& "$bt/aapt2.exe" link -I $androidJar --manifest (Join-Path $root 'AndroidManifest.xml') --min-sdk-version 33 --target-sdk-version 34 --version-code 1 --version-name '0.1.0' -A (Join-Path $root 'assets') -o "$work/unaligned.apk" "$work/resources.zip";Require-Exit 'Link resources'
Add-Type -AssemblyName System.IO.Compression
$archive=[IO.Compression.ZipFile]::Open("$work/unaligned.apk",[IO.Compression.ZipArchiveMode]::Update)
try{
    # Windows aapt2 can emit backslashes for nested asset paths. Normalize
    # before signing so assets/licenses/LICENSE is portable inside the APK.
    foreach($entry in @($archive.Entries)){
        if(-not $entry.FullName.Contains('\')){continue}
        $normalized=$entry.FullName.Replace('\','/')
        if($archive.GetEntry($normalized)){throw "Duplicate normalized APK entry: $normalized"}
        $input=$entry.Open();$memory=[IO.MemoryStream]::new()
        try{$input.CopyTo($memory);$payload=$memory.ToArray()}finally{$input.Dispose();$memory.Dispose()}
        $entry.Delete()
        $output=$archive.CreateEntry($normalized,[IO.Compression.CompressionLevel]::Optimal).Open()
        try{$output.Write($payload,0,$payload.Length)}finally{$output.Dispose()}
    }
    foreach($dex in Get-ChildItem "$work/dex" -Filter *.dex){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$dex.FullName,$dex.Name,[IO.Compression.CompressionLevel]::Optimal)|Out-Null}
}finally{$archive.Dispose()}
& "$bt/zipalign.exe" -f 4 "$work/unaligned.apk" "$work/aligned.apk";Require-Exit 'Zip alignment'
$key=Join-Path $build 'local-signing.jks'
if(-not(Test-Path -LiteralPath $key)){
    & "$jdk/keytool.exe" -genkeypair -noprompt -storetype PKCS12 -keystore $key -storepass android -keypass android -alias androiddebugkey -keyalg RSA -keysize 3072 -validity 10000 -dname 'CN=Local Creators HEIF module';Require-Exit 'Local test signing key'
}
$apk=Join-Path $dist 'creators-heif-wifi-0.1.0.apk'
& $java -jar "$bt/lib/apksigner.jar" sign --ks $key --ks-key-alias androiddebugkey --ks-pass pass:android --key-pass pass:android --v1-signing-enabled false --out $apk "$work/aligned.apk";Require-Exit 'APK signing'

# A non-installable probe JAR may include the API test declarations. The
# installable module above includes ONLY program.jar, never api.jar or tests.
& $javac --release 8 -cp $androidJar -d "$work/probe" (Join-Path $root 'tools\ContractProbe.java');Require-Exit 'Compile contract probe'
& $jar cf "$work/probe.jar" -C "$work/probe" .;Require-Exit 'Archive contract probe'
& $java -cp $d8 com.android.tools.r8.D8 --release --min-api 33 --lib $androidJar --output "$work/probe-dex" "$work/probe.jar" "$work/api.jar";Require-Exit 'D8 contract probe'
& $jar cf (Join-Path $dist 'creators-contract-probe.jar') -C "$work/probe-dex" classes.dex;Require-Exit 'Package contract probe'
[IO.File]::WriteAllText((Join-Path $build 'last-build-path.txt'),$work)
Get-FileHash -LiteralPath $apk -Algorithm SHA256 | Format-List Path,Hash
