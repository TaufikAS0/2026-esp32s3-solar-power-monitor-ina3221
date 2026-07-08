# Firmware Artifact `v0.16.7`

Release label: `wifi-sta-reconnect-stability`

Purpose:
- stop the device from falling back to AP mode after it has already connected successfully in STA mode
- keep reconnecting to the saved Wi-Fi instead of disappearing from `192.168.1.52` after a transient drop
- restart Arduino OTA cleanly after STA reconnect
- expose lightweight Wi-Fi and heap diagnostics in `/api/health` and `/api/status`

Likely root cause fixed in source:
- previous `wifi_service.cpp` used the initial STA connect timeout forever
- after any later disconnect, that stale timeout condition could send the board into setup AP mode
- once in AP mode, the board stopped serving the normal LAN IP and looked "offline" from the router side

Build result:
- app bin: `ESP32-SolarPower-CP.ino.bin`
- bootloader: `ESP32-SolarPower-CP.ino.bootloader.bin`
- partitions: `ESP32-SolarPower-CP.ino.partitions.bin`
- merged image: `ESP32-SolarPower-CP.ino.merged.bin`

Compile status:
- compiled successfully on July 8, 2026
- flash size used: about `1302876` bytes

Local deploy status:
- source fix compiled successfully
- local LAN probe to `192.168.1.52` still timed out during this session
- no setup SSID from the board was visible on this PC at the time of checking
- no ESP32 serial port was present on this PC, so OTA or USB flash could not be completed from the current state
