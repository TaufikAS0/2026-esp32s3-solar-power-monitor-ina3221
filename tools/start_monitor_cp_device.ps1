param(
    [string]$DeviceIp = "192.168.1.52",
    [string]$ComPort = "COM11",
    [string]$GrafanaUrl = "http://192.168.1.100:3000/d/solar-monitor-overview/solar-monitor-overview?orgId=1&from=now-24h&to=now&timezone=browser&var-device_id=`$__all&var-energy_source=backend&refresh=30s",
    [int]$DurationMinutes = 65,
    [int]$IntervalSeconds = 5,
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot ("artifacts\device-monitor\{0}" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$scriptPath = Join-Path $PSScriptRoot "monitor_cp_device.ps1"
$stdoutPath = Join-Path $OutputDir "stdout.log"
$stderrPath = Join-Path $OutputDir "stderr.log"
$command = "& '$scriptPath' -DeviceIp '$DeviceIp' -ComPort '$ComPort' -GrafanaUrl '$GrafanaUrl' -DurationMinutes $DurationMinutes -IntervalSeconds $IntervalSeconds -OutputDir '$OutputDir'"

$process = Start-Process -FilePath "powershell.exe" `
    -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", $command) `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -PassThru

[pscustomobject]@{
    pid = $process.Id
    output_dir = $OutputDir
    stdout = $stdoutPath
    stderr = $stderrPath
} | ConvertTo-Json -Compress
