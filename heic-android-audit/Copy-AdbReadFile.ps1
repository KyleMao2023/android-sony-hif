param(
    [Parameter(Mandatory)][string[]]$RemotePaths,
    [string]$DestinationDirectory = "$PSScriptRoot\samples",
    [string]$Device = 'ecef8f17'
)
$ErrorActionPreference = 'Stop'
[IO.Directory]::CreateDirectory($DestinationDirectory) | Out-Null
function Read-Exact([IO.Stream]$Stream, [int]$Count) {
    $buffer = [byte[]]::new($Count)
    $offset = 0
    while ($offset -lt $Count) {
        $n = $Stream.Read($buffer, $offset, $Count - $offset)
        if ($n -eq 0) { throw 'ADB connection closed unexpectedly' }
        $offset += $n
    }
    return ,$buffer
}
function Send-AdbRequest([IO.Stream]$Stream, [string]$Request) {
    $payload = [Text.Encoding]::UTF8.GetBytes($Request)
    $prefix = [Text.Encoding]::ASCII.GetBytes($payload.Length.ToString('x4'))
    $Stream.Write($prefix, 0, $prefix.Length)
    $Stream.Write($payload, 0, $payload.Length)
    $status = [Text.Encoding]::ASCII.GetString((Read-Exact $Stream 4))
    if ($status -ne 'OKAY') {
        $length = [Convert]::ToInt32([Text.Encoding]::ASCII.GetString((Read-Exact $Stream 4)), 16)
        throw [Text.Encoding]::UTF8.GetString((Read-Exact $Stream $length))
    }
}
foreach ($remotePath in $RemotePaths) {
    $destination = Join-Path $DestinationDirectory ([IO.Path]::GetFileName($remotePath))
    if (Test-Path -LiteralPath $destination) { throw "Destination already exists: $destination" }
    $client = [Net.Sockets.TcpClient]::new()
    $file = $null
    try {
        $client.Connect('127.0.0.1', 5037)
        $stream = $client.GetStream()
        $stream.ReadTimeout = 30000
        Send-AdbRequest $stream "host:transport:$Device"
        Send-AdbRequest $stream 'sync:'
        $pathBytes = [Text.Encoding]::UTF8.GetBytes($remotePath)
        $request = [Text.Encoding]::ASCII.GetBytes('RECV') + [BitConverter]::GetBytes([uint32]$pathBytes.Length) + $pathBytes
        $stream.Write($request, 0, $request.Length)
        $file = [IO.File]::Open($destination, [IO.FileMode]::CreateNew)
        while ($true) {
            $header = Read-Exact $stream 8
            $id = [Text.Encoding]::ASCII.GetString($header, 0, 4)
            $length = [BitConverter]::ToUInt32($header, 4)
            if ($id -eq 'DONE') { break }
            if ($id -eq 'FAIL') { throw [Text.Encoding]::UTF8.GetString((Read-Exact $stream $length)) }
            if ($id -ne 'DATA' -or $length -gt 65536) { throw "Unexpected sync packet: $id $length" }
            $data = Read-Exact $stream $length
            $file.Write($data, 0, $data.Length)
        }
    } finally {
        if ($file) { $file.Dispose() }
        $client.Dispose()
    }
    Get-FileHash -Algorithm SHA256 -LiteralPath $destination | Select-Object Path,Hash
}
