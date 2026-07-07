# Firmware Artifact `v0.16.2`

Release label: `m1-raw-history-fix`

Purpose:
- fix `1min` history view so it prefers browser raw history instead of trend/archive
- stop old 10-minute data from visually shifting because of browser-side re-sampling
- keep rendering tolerable on laptop browsers with very large raw datasets

Build result:
- app bin: `ESP32-SolarPower-CP.ino.bin`
- bootloader: `ESP32-SolarPower-CP.ino.bootloader.bin`
- partitions: `ESP32-SolarPower-CP.ino.partitions.bin`
- boot app0: `boot_app0.bin`
- merged image: `ESP32-SolarPower-CP.ino.merged.bin`
- flash arguments: `flash_args`

Compile status:
- compiled successfully on July 7, 2026
- flash size used: about `1296488` bytes

Important note:
- source changes for this artifact live on branch `feature/v0.12.0-sampling-controls`
- OTA attempt from home failed with `No response from the ESP`
- after that failed OTA attempt, device `192.168.1.52` stopped responding from the home network side
- this artifact was pushed so a local PC on the same site can continue the work
