# ESP32-S3 Solar Power Monitor CP

Firmware ESP32-S3 berbasis Arduino untuk monitoring jalur `solar`, `battery`, dan `load` memakai `INA3221AIRGVR` pada address `0x40`, plus dashboard web lokal, OTA, backend sender lokal, dan kontrol PWM LED dari browser.

Repo ini dipisahkan dari repo INA219 lama karena hardware sensing dan runtime control path sudah berubah cukup jauh.

## Stable Baseline

Baseline stabil saat repo ini dibuat:

- firmware version: `v0.11.1`
- release label: `esp32s3-load-shunt-voltage-fix`
- target board: `esp32:esp32:esp32s3`
- Wi-Fi default: `HardwareTest`
- IP kerja terakhir: `192.168.1.52`
- I2C: `SDA=21`, `SCL=20`
- sensor: `INA3221AIRGVR @ 0x40`
- shunt baseline:
  - `CH1 Solar = 5.00 mOhm`
  - `CH2 Battery = 5.00 mOhm`
  - `CH3 Load = 10.00 mOhm` (`R010`)
- LED PWM default:
  - pin `18`
  - frequency `100 Hz`
  - resolution `12-bit`
  - inverted output `true`

Detail baseline bench disimpan di [docs/BASELINE_v0.11.1.md](docs/BASELINE_v0.11.1.md).

## Struktur

```text
2026-Project-CP/
|-- ESP32-SolarPower-CP/
|   |-- ESP32-SolarPower-CP.ino
|   |-- firmware_version.h
|   |-- config.h
|   |-- ina_sensors.*
|   |-- wifi_service.*
|   |-- web_ui.*
|   `-- ...
`-- docs/
```

## Runtime Surfaces

- Serial boot log
- Local web dashboard
- `GET /api/health`
- `GET /api/status`
- `GET /api/config`
- OTA over Arduino OTA saat mode STA aktif

## Build Note

Firmware saat ini tidak lagi muat di skema partisi default `1.2MB APP`.

- target board tetap `esp32:esp32:esp32s3`
- compile/upload gunakan `PartitionScheme=min_spiffs`
- skema ini tetap menjaga OTA dan memberi slot app sekitar `1.9MB`
- jika memakai partisi default, build terbaru akan gagal atau image tidak akan boot

## Catatan Gitflow

Repo baru ini akan memakai pola:

- `main` untuk baseline stabil
- `develop` untuk integrasi
- `feature/*` untuk eksperimen dan fitur berikutnya

Langkah setelah baseline ini adalah membuka branch fitur untuk sampling cepat INA3221, buffer history lebih panjang, dan kontrol runtime penuh dari web dashboard.
