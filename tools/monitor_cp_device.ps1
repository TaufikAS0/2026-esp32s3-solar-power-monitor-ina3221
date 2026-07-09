param(
    [string]$DeviceIp = "192.168.1.52",
    [string]$ComPort = "COM11",
    [string]$GrafanaUrl = "http://192.168.1.100:3000/d/solar-monitor-overview/solar-monitor-overview?orgId=1&from=now-24h&to=now&timezone=browser&var-device_id=`$__all&var-energy_source=backend&refresh=30s",
    [int]$DurationMinutes = 65,
    [int]$IntervalSeconds = 5,
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

function Write-JsonLine {
    param(
        [string]$Path,
        [hashtable]$Object
    )

    $line = ($Object | ConvertTo-Json -Compress -Depth 10) + [Environment]::NewLine
    [System.IO.File]::AppendAllText($Path, $line)
}

function Append-Text {
    param(
        [string]$Path,
        [string]$Text
    )

    if ([string]::IsNullOrEmpty($Text)) {
        return
    }

    [System.IO.File]::AppendAllText($Path, $Text)
}

function Test-PingOnce {
    param([string]$HostName)

    try {
        $output = & ping -n 1 -w 1500 $HostName 2>$null
        return ($LASTEXITCODE -eq 0) -and ($output -match "TTL=")
    }
    catch {
        return $false
    }
}

function Invoke-JsonEndpoint {
    param(
        [string]$Url,
        [int]$TimeoutSeconds = 4
    )

    try {
        $response = & curl.exe --silent --show-error --connect-timeout $TimeoutSeconds --max-time $TimeoutSeconds $Url 2>$null
        if ($response -is [System.Array]) {
            $response = ($response -join "")
        }
        else {
            $response = [string]$response
        }
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($response)) {
            return $null
        }
        return $response | ConvertFrom-Json
    }
    catch {
        return $null
    }
}

function Invoke-HeadStatus {
    param(
        [string]$Url,
        [int]$TimeoutSeconds = 4
    )

    try {
        $response = & curl.exe --silent --show-error -I --connect-timeout $TimeoutSeconds --max-time $TimeoutSeconds $Url 2>$null
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($response)) {
            return [ordered]@{
                ok = $false
                status_line = ""
                location = ""
            }
        }

        $lines = ($response -split "`r?`n") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        $statusLine = if ($lines.Count -gt 0) { $lines[0].Trim() } else { "" }
        $locationLine = $lines | Where-Object { $_ -like "Location:*" } | Select-Object -First 1
        $location = if ($null -ne $locationLine) { $locationLine.Substring(9).Trim() } else { "" }
        return [ordered]@{
            ok = -not [string]::IsNullOrWhiteSpace($statusLine)
            status_line = $statusLine
            location = $location
        }
    }
    catch {
        return [ordered]@{
            ok = $false
            status_line = ""
            location = ""
        }
    }
}

function Invoke-LiveHistoryBurst {
    param(
        [string]$DeviceIp,
        [uint32]$AfterSequence,
        [int]$Limit = 512,
        [int]$MaxLoops = 4
    )

    $cursor = [uint32]$AfterSequence
    $pointCount = 0
    $loops = 0
    $ok = $false
    $overflowed = $false
    $truncated = $false
    $latestSeq = [uint32]$AfterSequence

    try {
        while ($loops -lt $MaxLoops) {
            $url = "http://{0}/api/history/live?after_seq={1}&limit={2}" -f $DeviceIp, $cursor, $Limit
            $data = Invoke-JsonEndpoint -Url $url
            if ($null -eq $data) {
                break
            }

            $ok = $true
            $loops++
            $overflowed = $overflowed -or [bool]$data.overflowed
            $truncated = [bool]$data.truncated

            $points = @()
            if ($null -ne $data.points) {
                $points = @($data.points)
            }

            $pointCount += $points.Count
            if ($points.Count -gt 0) {
                $lastPoint = $points[$points.Count - 1]
                if ($lastPoint -is [System.Collections.IList] -and $lastPoint.Count -gt 0) {
                    $latestSeq = [uint32]([int]$lastPoint[0])
                    $cursor = $latestSeq
                }
            }

            if (-not $truncated) {
                break
            }
        }
    }
    catch {
    }

    return [ordered]@{
        ok = $ok
        cursor = [uint64]$cursor
        point_count = $pointCount
        loops = $loops
        overflowed = $overflowed
        truncated = $truncated
        latest_seq = [uint64]$latestSeq
    }
}

function Open-SerialPort {
    param([string]$PortName)

    try {
        $port = New-Object System.IO.Ports.SerialPort $PortName, 115200, "None", 8, "one"
        $port.ReadTimeout = 250
        $port.DtrEnable = $true
        $port.RtsEnable = $true
        $port.Open()
        return $port
    }
    catch {
        return $null
    }
}

function Close-SerialPort {
    param($Port)

    if ($null -eq $Port) {
        return
    }

    try {
        if ($Port.IsOpen) {
            $Port.Close()
        }
    }
    catch {
    }
}

function Drain-SerialPort {
    param(
        $Port,
        [string]$SerialLogPath
    )

    if ($null -eq $Port -or -not $Port.IsOpen) {
        return 0
    }

    try {
        $text = $Port.ReadExisting()
        if ([string]::IsNullOrEmpty($text)) {
            return 0
        }

        Append-Text -Path $SerialLogPath -Text $text
        return $text.Length
    }
    catch {
        return 0
    }
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path "D:\Home Work\Firmware Github\2026-Project-CP\artifacts\device-monitor" $timestamp
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$summaryPath = Join-Path $OutputDir "monitor.jsonl"
$serialPath = Join-Path $OutputDir "serial.log"
$metaPath = Join-Path $OutputDir "meta.json"
$latestPath = Join-Path $OutputDir "latest.json"

$meta = [ordered]@{
    started_at = (Get-Date).ToString("o")
    device_ip = $DeviceIp
    com_port = $ComPort
    grafana_url = $GrafanaUrl
    duration_minutes = $DurationMinutes
    interval_seconds = $IntervalSeconds
    output_dir = $OutputDir
}
($meta | ConvertTo-Json -Depth 6) | Set-Content -Encoding UTF8 -Path $metaPath

$stopAt = (Get-Date).AddMinutes($DurationMinutes)
$nextConfigCheckAt = Get-Date "2000-01-01T00:00:00Z"
$nextGrafanaCheckAt = Get-Date "2000-01-01T00:00:00Z"
$configSnapshot = $null
$grafanaStatus = [ordered]@{
    ok = $false
    status_line = ""
    location = ""
}
$serialPort = $null
$serialWasOpen = $false
$lastReachability = ""
$iteration = 0
$historyCursor = [uint32]0
$historySnapshot = [ordered]@{
    ok = $false
    cursor = [uint64]0
    point_count = 0
    loops = 0
    overflowed = $false
    truncated = $false
    latest_seq = [uint64]0
}

Write-JsonLine -Path $summaryPath -Object ([ordered]@{
    ts = (Get-Date).ToString("o")
    kind = "monitor_event"
    event = "monitor_started"
    device_ip = $DeviceIp
    com_port = $ComPort
    output_dir = $OutputDir
})

try {
    while ((Get-Date) -lt $stopAt) {
        if ($null -eq $serialPort -or -not $serialPort.IsOpen) {
            $serialPort = Open-SerialPort -PortName $ComPort
            if ($null -ne $serialPort -and -not $serialWasOpen) {
                $serialWasOpen = $true
                Write-JsonLine -Path $summaryPath -Object ([ordered]@{
                    ts = (Get-Date).ToString("o")
                    kind = "monitor_event"
                    event = "serial_open"
                    com_port = $ComPort
                })
            }
        }

        $serialBytes = Drain-SerialPort -Port $serialPort -SerialLogPath $serialPath

        if ($null -eq $serialPort -or -not $serialPort.IsOpen) {
            if ($serialWasOpen) {
                $serialWasOpen = $false
                Write-JsonLine -Path $summaryPath -Object ([ordered]@{
                    ts = (Get-Date).ToString("o")
                    kind = "monitor_event"
                    event = "serial_closed"
                    com_port = $ComPort
                })
            }
        }

        $pingOk = Test-PingOnce -HostName $DeviceIp
        $health = Invoke-JsonEndpoint -Url ("http://{0}/api/health" -f $DeviceIp)
        $status = Invoke-JsonEndpoint -Url ("http://{0}/api/status" -f $DeviceIp)
        $historySnapshot = Invoke-LiveHistoryBurst -DeviceIp $DeviceIp -AfterSequence $historyCursor
        if ($historySnapshot.ok) {
            $historyCursor = [uint32]$historySnapshot.cursor
        }

        if ((Get-Date) -ge $nextConfigCheckAt) {
            $config = Invoke-JsonEndpoint -Url ("http://{0}/api/config" -f $DeviceIp)
            if ($null -ne $config) {
                $configSnapshot = $config
            }
            $nextConfigCheckAt = (Get-Date).AddSeconds(60)
        }

        if ((Get-Date) -ge $nextGrafanaCheckAt) {
            $grafanaStatus = Invoke-HeadStatus -Url $GrafanaUrl
            $nextGrafanaCheckAt = (Get-Date).AddSeconds(30)
        }

        $backendApiHost = $null
        $backendApiPort = $null
        $backendApiHostPingOk = $null
        if ($null -ne $configSnapshot -and -not [string]::IsNullOrWhiteSpace($configSnapshot.api_base)) {
            try {
                $apiUri = [System.Uri]$configSnapshot.api_base
                $backendApiHost = $apiUri.Host
                $backendApiPort = $apiUri.Port
                if ($backendApiHost -and $backendApiHost -notin @("127.0.0.1", "localhost")) {
                    $backendApiHostPingOk = Test-PingOnce -HostName $backendApiHost
                }
            }
            catch {
            }
        }

        $reachability = if ($health) {
            "http_ok"
        }
        elseif ($pingOk) {
            "ping_only"
        }
        else {
            "offline"
        }

        if ($reachability -ne $lastReachability) {
            $lastReachability = $reachability
            Write-JsonLine -Path $summaryPath -Object ([ordered]@{
                ts = (Get-Date).ToString("o")
                kind = "monitor_event"
                event = "reachability_changed"
                reachability = $reachability
                ping_ok = $pingOk
                health_ok = [bool]$health
                status_ok = [bool]$status
            })
        }

        $summary = [ordered]@{
            ts = (Get-Date).ToString("o")
            kind = "sample"
            seq = $iteration
            device_ip = $DeviceIp
            com_port = $ComPort
            ping_ok = $pingOk
            health_ok = [bool]$health
            status_ok = [bool]$status
            history_live_ok = [bool]$historySnapshot.ok
            history_live_points = $historySnapshot.point_count
            history_live_loops = $historySnapshot.loops
            history_live_truncated = [bool]$historySnapshot.truncated
            history_live_overflowed = [bool]$historySnapshot.overflowed
            history_live_last_seq = [uint64]$historySnapshot.latest_seq
            serial_open = [bool]($serialPort -and $serialPort.IsOpen)
            serial_bytes = $serialBytes
            grafana_ok = $grafanaStatus.ok
            grafana_status_line = $grafanaStatus.status_line
            grafana_location = $grafanaStatus.location
            firmware_version = if ($health) { $health.firmware_version } else { $null }
            release_label = if ($health) { $health.release_label } else { $null }
            uptime_ms = if ($health) { $health.uptime_ms } else { $null }
            wifi_mode = if ($health) { $health.wifi_mode } else { $null }
            wifi_sta_connected = if ($health) { $health.wifi_sta_connected } else { $null }
            wifi_disconnect_count = if ($health) { $health.wifi_disconnect_count } else { $null }
            wifi_reconnect_attempts = if ($health) { $health.wifi_reconnect_attempts } else { $null }
            wifi_last_event_name = if ($health) { $health.wifi_last_event_name } else { $null }
            wifi_last_event_ms = if ($health) { $health.wifi_last_event_ms } else { $null }
            wifi_last_disconnect_reason = if ($health) { $health.wifi_last_disconnect_reason } else { $null }
            wifi_last_disconnect_reason_name = if ($health) { $health.wifi_last_disconnect_reason_name } else { $null }
            wifi_last_disconnect_rssi = if ($health) { $health.wifi_last_disconnect_rssi } else { $null }
            reset_reason = if ($health) { $health.reset_reason } else { $null }
            reset_reason_code = if ($health) { $health.reset_reason_code } else { $null }
            free_heap_bytes = if ($health) { $health.free_heap_bytes } else { $null }
            min_free_heap_bytes = if ($health) { $health.min_free_heap_bytes } else { $null }
            sensor_measured_hz = if ($health) { $health.sensor_measured_hz } else { $null }
            backend_state = if ($status) { $status.backend_sender_state } else { $null }
            backend_queue_depth = if ($status) { $status.backend_queue_depth } else { $null }
            backend_retry_batch_depth = if ($status) { $status.backend_retry_batch_depth } else { $null }
            backend_last_http_status = if ($status) { $status.backend_last_http_status } else { $null }
            backend_last_error = if ($status) { $status.backend_last_error } else { $null }
            backend_next_retry_in_ms = if ($status) { $status.backend_next_retry_in_ms } else { $null }
            backend_api_base = if ($configSnapshot) { $configSnapshot.api_base } else { $null }
            backend_api_host = $backendApiHost
            backend_api_port = $backendApiPort
            backend_api_host_ping_ok = $backendApiHostPingOk
        }

        $latestJson = $summary | ConvertTo-Json -Depth 8
        $latestJson | Set-Content -Encoding UTF8 -Path $latestPath
        Write-JsonLine -Path $summaryPath -Object $summary

        $iteration++
        Start-Sleep -Seconds $IntervalSeconds
    }
}
finally {
    $serialBytes = Drain-SerialPort -Port $serialPort -SerialLogPath $serialPath
    Close-SerialPort -Port $serialPort
    Write-JsonLine -Path $summaryPath -Object ([ordered]@{
        ts = (Get-Date).ToString("o")
        kind = "monitor_event"
        event = "monitor_stopped"
        serial_bytes = $serialBytes
        output_dir = $OutputDir
    })
}
