# Run once in a normal PowerShell session; the agent sandbox protects .git.
$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$safeRoot=$projectRoot.Replace('\','/')
$helperState=Join-Path $projectRoot '.work'
[IO.Directory]::CreateDirectory($helperState)|Out-Null
$emptyExclude=Join-Path $helperState 'git-empty-exclude'
[IO.File]::WriteAllText($emptyExclude,'')
function Invoke-RepoGit {
    & git -c "safe.directory=$safeRoot" -c "core.excludesFile=$emptyExclude" -C $projectRoot @args
    if($LASTEXITCODE -ne 0){throw "git failed: $args"}
}
if(-not(Test-Path -LiteralPath (Join-Path $projectRoot '.git'))){Invoke-RepoGit init -b main}
$actualRoot=(Invoke-RepoGit rev-parse --show-toplevel).Trim()
if(-not [string]::Equals($actualRoot,$safeRoot,[StringComparison]::OrdinalIgnoreCase)){
    throw 'Refusing to stage files in a different repository'
}
& git -c "safe.directory=$safeRoot" -c "core.excludesFile=$emptyExclude" -C $projectRoot rev-parse --verify --quiet HEAD
if($LASTEXITCODE -eq 0){
    Write-Output 'Repository already has a commit; leaving its history unchanged.'
    Invoke-RepoGit status --short --branch
    return
}
Invoke-RepoGit add -- .gitignore .gitattributes README.md Initialize-Repository.ps1 sony-heif-module heic-android-audit/Invoke-AdbRead.ps1 heic-android-audit/Copy-AdbReadFile.ps1
Invoke-RepoGit diff --cached --check
$staged=@(Invoke-RepoGit diff --cached --name-only)
$unexpected=@($staged | Where-Object {
    $_ -match '(^|/)(target|build|dist|diagnostics|creators-app-audit|\.tools|\.work)/|/docs/evidence|\.(apk|dex|exe|so|HIF|hif|ppm|zip|gz|jks|keystore)$'
})
if($unexpected.Count){throw "Unexpected staged paths: $($unexpected -join ', ')"}
if(-not $staged.Count){throw 'No source files staged'}
Write-Output "Reviewed staged paths: $($staged.Count)"
Invoke-RepoGit commit -m 'Initial Sony HEIF Entry 0.1.3 baseline'
& git -c "safe.directory=$safeRoot" -c "core.excludesFile=$emptyExclude" -C $projectRoot show-ref --verify --quiet refs/tags/v0.1.3
if($LASTEXITCODE -eq 1){Invoke-RepoGit tag -a v0.1.3 -m 'Sony HEIF Entry 0.1.3: Gallery and WeChat baseline'}
elseif($LASTEXITCODE -ne 0){throw 'Could not inspect the baseline tag'}
Invoke-RepoGit status --short --branch
Invoke-RepoGit log -1 --oneline
