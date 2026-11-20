# Kipas Otomatis DHT22 — ESP32 + DHT22 + Relay + LCD I2C

Kipas menyala sendiri saat ruangan panas, mati sendiri saat sudah sejuk.
Sensor DHT22 membaca suhu tiap 2 detik; suhu >= 30 C kipas ON, suhu
<= 28 C kipas OFF (hysteresis 2 C, supaya relay tidak cetak-cetek di
sekitar ambang). LCD menampilkan suhu + status kipas, LED hijau menyala
saat kipas berputar, dan notifikasi Telegram terkirim setiap kipas
berubah status (bisa dimatikan dengan `USE_TELEGRAM` 0).

Video penjelasan (60 detik):
https://youtube.com/shorts/5kWmxbIPNww

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| DHT22 | Sensor suhu + kelembapan |
| Relay 1 kanal | Saklar kipas (aktif HIGH) |
| Kipas DC 12V + adaptor 12V | Beban yang dikendalikan relay |
| LCD 16x2 I2C (0x27) | Tampil suhu + status kipas |
| LED hijau | Menyala saat kipas berputar (seri 220 Ω ke GND) |

## Wiring

| Dari | Ke |
|---|---|
| DHT22 DATA | ESP32 GPIO4 |
| DHT22 VCC | ESP32 3V3 |
| LCD SDA / SCL | ESP32 GPIO21 / GPIO22 (bus I2C) |
| LCD VCC | ESP32 VIN 5V |
| Relay IN | ESP32 GPIO26 |
| Relay VCC | ESP32 VIN 5V |
| Relay NO | Kipas DC (+) |
| Relay COM | Adaptor 12V (+) |
| Kipas DC (−) | Adaptor 12V (−) |
| LED hijau | ESP32 GPIO32 (seri 220 Ω ke GND) |
| Semua GND | Digabung |

Skema lengkap: [schematic.png](schematic.png)

## Cara kerja

1. ESP32 membaca suhu dari DHT22 tiap 2 detik.
2. Suhu >= 30 C → relay menutup, kipas menyala, LED hijau menyala,
   LCD tampil "Kipas: NYALA", Telegram terkirim.
3. Suhu <= 28 C → relay membuka, kipas mati, LCD tampil "Kipas: MATI",
   Telegram terkirim.
4. Di antara 28–30 C, kipas tetap pada kondisi terakhir (hysteresis).
5. DHT gagal dibaca (NaN) → LCD tampil "Sensor error", kipas tetap
   pada kondisi terakhir, alat tidak hang/crash.

## Konfigurasi (di awal sketch)

- `USE_TELEGRAM` (1) — kirim notifikasi Telegram saat kipas berubah
  status. Set 0 untuk tanpa Telegram (WiFi tidak dipakai).
- `SUHU_NYALA` (30.0) — ambang kipas ON (derajat Celcius).
- `SUHU_MATI` (28.0) — ambang kipas OFF (derajat Celcius).
- `INTERVAL_BACA_MS` (2000) — baca suhu tiap 2 detik.
- `WIFI_SSID` / `WIFI_PASS` — WiFi rumahmu.
- `BOT_TOKEN` / `CHAT_ID` — token bot dan chat ID Telegram.
- `RELAY_AKTIF` (HIGH) — ubah ke LOW kalau modul relay-mu aktif LOW.

## Library yang dibutuhkan

| Library | Versi | Keterangan |
|---|---|---|
| DHT sensor library | 1.4.7 | Sensor DHT22 |
| Adafruit Unified Sensor | 1.1.15 | Dependensi DHT |
| LiquidCrystal_I2C | 1.1.2 | LCD 16x2 I2C |
| UniversalTelegramBot | 1.3.0 | Notifikasi Telegram |
| ArduinoJson | 6.21.5 | Dependensi TelegramBot (WAJIB v6 — v7 tidak dipakai di sini) |

Semua library di atas juga tersedia sebagai ZIP di folder [`libraries/`](libraries/)
— install lewat Arduino IDE: Sketch → Include Library → Add .ZIP Library.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 KipasOtomatis
```

Terverifikasi 20 Nov 2026: sketch 1.090.525 byte (83% flash),
RAM 51.936 byte (15%), ESP32 core 4.0.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
