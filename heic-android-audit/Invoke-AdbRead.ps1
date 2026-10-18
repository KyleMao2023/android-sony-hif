param(
    [Parameter(Mandatory)][string]$Command,
    [string]$Device = 'ecef8f17',
    [string]$OutputPath
)
$ErrorActionPreference = 'Stop'

function Read-Exact([System.IO.Stream]$Stream, [int]$Count) {
    $buffer = [byte[]]::new($Count)
    $offset = 0
    while ($offset -lt $Count) {
        $n = $Stream.Read($buffer, $offset, $Count - $offset)
        if ($n -eq 0) { throw 'ADB connection closed unexpectedly' }
        $offset += $n
    }
    return ,$buffer
}

function Send-AdbRequest([System.IO.Stream]$Stream, [string]$Request) {
    $payload = [Text.Encoding]::UTF8.GetBytes($Request)
    $prefix = [Text.Encoding]::ASCII.GetBytes($payload.Length.ToString('x4'))
    $Stream.Write($prefix, 0, $prefix.Length)
    $Stream.Write($payload, 0, $payload.Length)
    $status = [Text.Encoding]::ASCII.GetString((Read-Exact $Stream 4))
    if ($status -ne 'OKAY') {
        $length = [Convert]::ToInt32([Text.Encoding]::ASCII.GetString((Read-Exact $Stream 4)), 16)
        $message = [Text.Encoding]::UTF8.GetString((Read-Exact $Stream $length))
        throw "ADB $status : $message"
    }
}

$client = [System.Net.Sockets.TcpClient]::new()
try {
    $client.Connect('127.0.0.1', 5037)
    $stream = $client.GetStream()
    $stream.ReadTimeout = 30000
    Send-AdbRequest $stream "host:transport:$Device"
    Send-AdbRequest $stream "shell:$Command"
    $result = [System.IO.MemoryStream]::new()
    $buffer = [byte[]]::new(65536)
    while (($count = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
        $result.Write($buffer, 0, $count)
    }
    $output = [Text.Encoding]::UTF8.GetString($result.ToArray())
    if ($OutputPath) {
        [IO.File]::WriteAllText($OutputPath, "Command: $Command`n`n$output", [Text.UTF8Encoding]::new($false))
    }
    $output
} finally {
    $client.Dispose()
}
