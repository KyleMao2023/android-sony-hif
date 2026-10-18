param([Parameter(Mandatory)][string]$LocalPath,[Parameter(Mandatory)][string]$RemoteName,[string]$Device='ecef8f17')
$ErrorActionPreference='Stop'
if($RemoteName -notmatch '^[A-Za-z0-9_.-]+$'){throw 'RemoteName must be a simple test filename'}
$remote='/data/local/tmp/sony-heif-entry-test/'+$RemoteName
function Read-Exact([IO.Stream]$stream,[int]$count){
    $bytes=[byte[]]::new($count);$pos=0
    while($pos -lt $count){$n=$stream.Read($bytes,$pos,$count-$pos);if(!$n){throw 'ADB EOF'};$pos+=$n}
    return ,$bytes
}
function Request([IO.Stream]$stream,[string]$request){
    $bytes=[Text.Encoding]::UTF8.GetBytes($request);$prefix=[Text.Encoding]::ASCII.GetBytes($bytes.Length.ToString('x4'))
    $stream.Write($prefix);$stream.Write($bytes)
    $status=[Text.Encoding]::ASCII.GetString((Read-Exact $stream 4));if($status -ne 'OKAY'){throw "ADB request $status"}
}
function Packet([IO.Stream]$stream,[string]$id,[byte[]]$data){
    $stream.Write([Text.Encoding]::ASCII.GetBytes($id));$stream.Write([BitConverter]::GetBytes([uint32]$data.Length));$stream.Write($data)
}
$client=[Net.Sockets.TcpClient]::new()
try{
    $client.Connect('127.0.0.1',5037);$stream=$client.GetStream();$stream.ReadTimeout=30000
    Request $stream "host:transport:$Device";Request $stream 'sync:'
    Packet $stream 'SEND' ([Text.Encoding]::UTF8.GetBytes($remote+',33261'))
    $file=[IO.File]::OpenRead((Resolve-Path -LiteralPath $LocalPath))
    try{
        $bytes=[byte[]]::new(65536)
        while(($n=$file.Read($bytes,0,$bytes.Length)) -gt 0){
            $stream.Write([Text.Encoding]::ASCII.GetBytes('DATA'));$stream.Write([BitConverter]::GetBytes([uint32]$n));$stream.Write($bytes,0,$n)
        }
    }finally{$file.Dispose()}
    $stream.Write([Text.Encoding]::ASCII.GetBytes('DONE'));$stream.Write([BitConverter]::GetBytes([uint32][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()))
    $response=Read-Exact $stream 8;$status=[Text.Encoding]::ASCII.GetString($response,0,4)
    if($status -ne 'OKAY'){$n=[BitConverter]::ToUInt32($response,4);throw ([Text.Encoding]::UTF8.GetString((Read-Exact $stream $n)))}
    "Uploaded $remote"
}finally{$client.Dispose()}
