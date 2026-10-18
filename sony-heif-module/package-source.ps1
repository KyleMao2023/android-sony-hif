$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$versionLine=Get-Content -LiteralPath (Join-Path $projectRoot 'module\module.prop') | Where-Object {$_ -match '^version='}
$version=($versionLine -replace '^version=','').Trim()
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid module version'}
$files=[Collections.Generic.List[IO.FileInfo]]::new()
foreach($dir in @('native','tests','tools','module','docs','rust\src','rust\vendor\heic-rs\src','rust\.cargo')){
    Get-ChildItem -LiteralPath (Join-Path $projectRoot $dir) -Recurse -File | ForEach-Object {$files.Add($_)}
}
Get-ChildItem -LiteralPath $projectRoot -File | Where-Object {$_.Name -match '\.(md|ps1)$|^(LICENSE|NOTICE)'} | ForEach-Object {$files.Add($_)}
foreach($name in @('rust\Cargo.toml','rust\Cargo.lock')){$files.Add((Get-Item -LiteralPath (Join-Path $projectRoot $name)))}
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'rust\vendor\heic-rs') -File | Where-Object {$_.Name -match '^Cargo.toml$|^README|^(LICENSE|NOTICE)'} | ForEach-Object {$files.Add($_)}
$selected=@($files | Where-Object {$_.Extension -in @('.cpp','.h','.hpp','.map','.rs','.toml','.lock','.md','.ps1','.sh','.prop','.txt','.json') -or $_.Name -match '^(LICENSE|NOTICE|skip_mount)'} | Sort-Object FullName -Unique)
$dist=Join-Path $projectRoot 'dist'
[IO.Directory]::CreateDirectory($dist)|Out-Null
$zip=Join-Path $dist ('sony-heif-entry-'+$version+'-source.zip')
Add-Type -AssemblyName System.IO.Compression
# Never replace a prior source deliverable silently.
$zipStream=[IO.File]::Open($zip,[IO.FileMode]::CreateNew)
$archive=[IO.Compression.ZipArchive]::new($zipStream,[IO.Compression.ZipArchiveMode]::Create)
try{
    foreach($file in $selected){
        $relative=[IO.Path]::GetRelativePath($projectRoot,$file.FullName).Replace('\','/')
        if($relative.StartsWith('../')){throw 'Source path escaped project'}
        $entry=$archive.CreateEntry('sony-heif-module/'+$relative,[IO.Compression.CompressionLevel]::Optimal)
        $output=$entry.Open();$input=[IO.File]::OpenRead($file.FullName)
        try{$input.CopyTo($output)}finally{$input.Dispose();$output.Dispose()}
    }
}finally{$archive.Dispose();$zipStream.Dispose()}
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | Format-List Path,Hash
"Source entries: $($selected.Count)"
