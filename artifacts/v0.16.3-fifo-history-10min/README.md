# Firmware Artifact `v0.16.3`

Release label: `fifo-history-10min`

Purpose:
- stop `1min` history from appearing to wipe all cached data when raw ESP history rolls over
- keep `1min` on browser raw history with stable FIFO-style eviction instead of full reset
- add `10min` history mode backed by device archive snapshots every `10 s`
- expose a `100 min` archive window from the `600`-slot minute-history buffer

Build result:
- app bin: `ESP32-SolarPower-CP.ino.bin`
- bootloader: `ESP32-SolarPower-CP.ino.bootloader.bin`
- partitions: `ESP32-SolarPower-CP.ino.partitions.bin`
- merged bin: `ESP32-SolarPower-CP.ino.merged.bin`

Compile status:
- compiled successfully on July 8, 2026
- flash size used: about `1299556` bytes

Verification summary:
- OTA upload to `192.168.1.52` succeeded from the local PC
- `/api/health` reports `v0.16.3` and release label `fifo-history-10min`
- `/api/history?view=minutes` now reports `interval_ms=10000` and `history_minutes_span_ms=6000000`
