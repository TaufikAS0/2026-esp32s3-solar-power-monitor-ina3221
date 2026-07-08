## v0.16.6 raw-fifo-stable-archive

Build date: 2026-07-08

Purpose:
- Keep raw `1s`, `3s`, `10s`, and `1min` behavior unchanged.
- Fix `10min` archive mode so it does not look odd from single accidental 10-second spike captures.

What changed:
- `10min` archive no longer stores one direct instantaneous sample every 10 seconds.
- Archive points now use representative values from the recent 10-second seconds-history window.
- This reduces isolated archive spikes that made `10min` look inconsistent versus `1s` through `1min`.
- Firmware version bumped to `v0.16.6`.
- Release label bumped to `raw-fifo-stable-archive`.

Build result:
- `arduino-cli compile` succeeded for `esp32:esp32:esp32s3`
- Sketch uses `1301452` bytes (`99%`) of program storage.

Deploy result:
- OTA to `192.168.1.52` completed successfully on 2026-07-08.
- `/api/health` now reports `v0.16.6` and `raw-fifo-stable-archive`.
- `/api/history?view=minutes` now reports:
  - `sample_note: Long archive stores representative 10 s bucket snapshots in FIFO order.`

Files:
- `ESP32-SolarPower-CP.ino.bin`
- `ESP32-SolarPower-CP.ino.bootloader.bin`
- `ESP32-SolarPower-CP.ino.partitions.bin`
- `ESP32-SolarPower-CP.ino.merged.bin`
- `boot_app0.bin`
