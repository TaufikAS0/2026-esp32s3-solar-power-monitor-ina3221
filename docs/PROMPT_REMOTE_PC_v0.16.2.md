Gunakan prompt ini di Codex pada PC yang berada di lokasi device:

```text
Kerjakan repo ini:
D:\JIN Work\Document\0. JIN Project Github Programming\2026 ESP32 Solar Power Monitor\2026-Project-CP

Branch target:
feature/v0.12.0-sampling-controls

Tolong baca dulu:
1. README.md
2. docs/FEATURE_v0.12.0_SAMPLING_CONTROLS.md jika ada konteks yang relevan
3. artifacts/v0.16.2-m1-raw-history-fix/README.md

Konteks penting:
- Firmware source lokal sudah diubah ke v0.16.2 dengan label m1-raw-history-fix.
- Tujuan fix ini adalah membuat mode 1min memakai browser raw history 10 menit, bukan trend/archive, supaya data lama tidak terlihat berubah-ubah atau seperti di-average ulang saat data baru masuk.
- Hasil compile bin sudah ada di:
  artifacts/v0.16.2-m1-raw-history-fix/
- Dari lokasi rumah, OTA ke 192.168.1.52 gagal dengan "No response from the ESP", lalu board tidak lagi merespons dari sisi sana. Karena kamu ada di PC lokal dekat device, lanjutkan dari sana.

Tugasmu:
1. Pastikan branch sudah up to date dan cek status repo.
2. Verifikasi source fix di:
   - ESP32-SolarPower-CP/web_ui.cpp
   - ESP32-SolarPower-CP/firmware_version.h
3. Cek apakah device 192.168.1.52 masih online dari jaringan lokal sana.
4. Jika device online dan OTA aktif, upload firmware v0.16.2 ke board.
5. Jika OTA tidak bisa, gunakan jalur lokal yang paling realistis di PC sana:
   - serial upload, atau
   - flash dengan bin artifacts yang sudah ada
6. Setelah device hidup, verifikasi:
   - /api/health menunjukkan v0.16.2 dan release label m1-raw-history-fix
   - mode 1min di web tidak lagi terlihat mengaverage atau mengubah bentuk data lama
   - jika masih ada bug, perbaiki langsung di source
7. Jika kamu membuat perbaikan tambahan, compile ulang, upload lagi, lalu commit dan push ke branch yang sama dengan pesan commit yang jelas.

Yang ingin saya dapatkan darimu:
- status device saat ini
- cara upload yang akhirnya berhasil
- hasil verifikasi mode 1min
- jika ada bug sisa, akar masalahnya apa
- commit hash terakhir jika ada perubahan baru

Jangan berhenti di analisa saja. Lanjutkan sampai upload atau sampai blocker lokal yang benar-benar nyata ditemukan.
```
