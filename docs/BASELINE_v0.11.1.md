# Baseline v0.11.1

Tanggal baseline: `2026-07-07`

## Tujuan

Menyimpan titik balik stabil pertama untuk varian firmware `ESP32-S3 + INA3221AIRGVR` sebelum tuning sampling cepat dan kontrol runtime yang lebih agresif ditambahkan.

## Firmware

- version: `v0.11.1`
- release label: `esp32s3-load-shunt-voltage-fix`
- sketch path: `ESP32-SolarPower-CP/`

## Hardware Mapping

- board: `ESP32-S3`
- I2C `SDA=21`
- I2C `SCL=20`
- sensor: `INA3221AIRGVR`
- sensor address: `0x40`
- channel map:
  - `CH1 = Solar`
  - `CH2 = Battery`
  - `CH3 = Load`

## Shunt Baseline

- `Solar = 5.00 mOhm`
- `Battery = 5.00 mOhm`
- `Load = 10.00 mOhm` (`R010`)

## Network Baseline

- SSID default: `HardwareTest`
- password default: `jayaabadi100`
- IP bench terakhir: `192.168.1.52`
- OTA dipakai di jaringan STA

## Behavior Snapshot

- dashboard lokal aktif
- Wi-Fi config sudah bisa diubah dari web server
- INA shunt config sudah bisa diubah dari web server
- LED PWM dan flash timing sudah bisa dikontrol dari web server
- firmware membaca `bus voltage` sebagai tegangan jalur utama load agar pembacaan tidak membesar karena `bus + shunt`

## Next Step After This Baseline

- tambah mode sampling cepat INA3221 dengan agregasi per interval report
- perpanjang buffer history grafik
- tampilkan semua setting runtime yang relevan langsung di dashboard
