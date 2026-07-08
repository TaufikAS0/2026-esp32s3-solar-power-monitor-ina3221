## v0.16.5 raw-fifo-stable-ui

Build date: 2026-07-08

Purpose:
- Keep browser raw FIFO behavior.
- Reduce live UI instability caused by rendering very large raw windows.
- Preserve real sample picks for long raw views without browser-side averaging.

Changes:
- `1min` raw history view now compacts display points with stable time buckets.
- Compaction uses actual raw samples from each bucket and reports `stable raw pick | no avg` in the UI note.
- Firmware version bumped to `v0.16.5`.
- Release label bumped to `raw-fifo-stable-ui`.

Build result:
- `arduino-cli compile` succeeded for `esp32:esp32:esp32s3`
- Sketch uses `1300548` bytes (`99%`) of program storage.

Files:
- `ESP32-SolarPower-CP.ino.bin`
- `ESP32-SolarPower-CP.ino.bootloader.bin`
- `ESP32-SolarPower-CP.ino.partitions.bin`
- `ESP32-SolarPower-CP.ino.merged.bin`
- `boot_app0.bin`

Live deploy note:
- OTA attempt to `192.168.1.52` transferred the image, then ended with `No response from device`.
- After the attempt, the board answered ping intermittently but HTTP port `80` and OTA port `3232` were closed.
- No working ESP32 serial port was available on this PC at the time of recovery, so live verification of `v0.16.5` is still pending.
