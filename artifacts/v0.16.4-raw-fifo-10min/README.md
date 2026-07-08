# Firmware Artifact `v0.16.4`

Release label: `raw-fifo-10min`

Purpose:
- keep `1s`, `3s`, `10s`, and `1min` views on full browser raw history without browser-side decimation
- keep FIFO behavior when raw ESP history rolls over, without clearing all browser history
- keep `10min` mode as device archive history with `10 s` snapshots across `100 min`

Build result:
- app bin: `ESP32-SolarPower-CP.ino.bin`
- bootloader: `ESP32-SolarPower-CP.ino.bootloader.bin`
- partitions: `ESP32-SolarPower-CP.ino.partitions.bin`
- merged bin: `ESP32-SolarPower-CP.ino.merged.bin`
- boot app0: `boot_app0.bin`

Compile status:
- compiled successfully on July 8, 2026
- flash size used: about `1298708` bytes

Verification summary:
- OTA upload to `192.168.1.52` succeeded from the local PC
- `/api/health` reports `v0.16.4` and release label `raw-fifo-10min`
- `1min` view stays on `Source live burst` and no longer shows browser decimation (`-> chart pts`)
- `10min` view shows `Window 100.0 min` and `Source device archive`
