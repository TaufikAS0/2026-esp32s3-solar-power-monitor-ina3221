# Feature v0.12.0 - Sampling Runtime Controls

Tanggal verifikasi: `2026-07-07`

## Ringkasan

Build `v0.12.0` menambahkan kontrol runtime sampling yang dipisah dari dashboard web:

- `sensor_poll_interval_ms`
- `sample_interval_ms` untuk report/send
- `history_interval_ms`
- `chart_point_limit`
- `ina_averaging_samples`
- `ina_bus_conv_us`
- `ina_shunt_conv_us`

## Default Baru

- sensor poll: `1 ms`
- report/send: `1000 ms`
- history sample: `100 ms`
- chart point limit: `180`
- history capacity: `300`
- I2C clock: `400000 Hz`
- INA config register: `0x7007`
- INA frame time teoritis: `840 us`
- INA rate teoritis per channel: `~1190.5 Hz`

## Dashboard / API

Field runtime baru sudah tampil di:

- `GET /api/health`
- `GET /api/status`
- `GET /api/config`
- `GET /api/history`
- halaman dashboard utama `/`
- `POST /api/config/runtime`

## Catatan Device Bench

Saat verifikasi OTA ke `192.168.1.52`, device kembali online dengan:

- firmware `v0.12.0`
- release label `max-sampling-runtime-controls`
- measured sensor read rate sekitar `300-700 Hz` tergantung kondisi loop runtime
- shunt yang tersimpan di Preferences:
  - `Solar = 10.00 mOhm`
  - `Battery = 5.00 mOhm`
  - `Load = 5.00 mOhm`

Artinya nilai shunt yang tampil setelah OTA tetap mengikuti config yang sudah ada di device, bukan langsung dipaksa ke default source code.
