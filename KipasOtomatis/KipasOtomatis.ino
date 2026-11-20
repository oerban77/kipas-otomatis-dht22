/*
  Kipas Otomatis DHT22 — ESP32 + DHT22 + Relay + LCD 16x2 I2C + Telegram
  ========================================================================
  Kipas menyala sendiri saat ruangan panas, mati sendiri saat sudah sejuk.
  Sensor DHT22 membaca suhu tiap 2 detik; suhu >= 30 C kipas ON,
  suhu <= 28 C kipas OFF (hysteresis 2 C, supaya relay tidak cetak-cetek
  di sekitar ambang). LCD menampilkan suhu + status kipas. Setiap kipas
  berubah status, notifikasi Telegram terkirim (bisa dimatikan).

  Wiring:
    DHT22 DATA  -> ESP32 GPIO4  (VCC 3V3, GND digabung)
    Relay IN    -> ESP32 GPIO26 (VCC VIN 5V, GND digabung)
    Relay NO    -> Kipas DC (adaptor 12V, lihat skema)
    LCD SDA     -> ESP32 GPIO21, SCL -> GPIO22, VCC VIN 5V
    LED hijau   -> ESP32 GPIO32 (seri resistor 220 ohm ke GND)

  Library yang dibutuhkan (versi terverifikasi):
    DHT sensor library 1.4.7, Adafruit Unified Sensor 1.1.15,
    LiquidCrystal_I2C 1.1.2, UniversalTelegramBot 1.3.0,
    ArduinoJson 6.21.5 (WAJIB v6 — v7 tidak dipakai di sini)

  Compile terverifikasi: arduino-cli compile --fqbn esp32:esp32:esp32
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ---------------------------------------------------------------------------
// KONFIGURASI — ubah di sini saja
// ---------------------------------------------------------------------------

// Saklar fitur: 1 = kirim notifikasi Telegram saat kipas berubah status,
// 0 = tanpa Telegram (WiFi tidak dipakai sama sekali).
#define USE_TELEGRAM 1

// Pin
const uint8_t PIN_DHT = 4;            // Data sensor DHT22
const uint8_t PIN_RELAY_KIPAS = 26;   // Sinyal relay kipas
const uint8_t PIN_LED_HIJAU = 32;     // LED penanda kipas menyala
const uint8_t PIN_SDA = 21;           // Bus I2C LCD
const uint8_t PIN_SCL = 22;           // Bus I2C LCD

// LCD 16x2 I2C
const uint8_t ALAMAT_LCD_I2C = 0x27;
const uint8_t KOLOM_LCD = 16;
const uint8_t BARIS_LCD = 2;

// Ambang suhu (derajat Celcius) — hysteresis 2 C
const float SUHU_NYALA = 30.0;  // suhu >= ini -> kipas ON
const float SUHU_MATI = 28.0;   // suhu <= ini -> kipas OFF

// Waktu
const unsigned long INTERVAL_BACA_MS = 2000;    // baca suhu tiap 2 detik
const unsigned long TIMEOUT_WIFI_MS = 15000;    // maks. tunggu WiFi 15 detik
const unsigned long JEDA_LCD_MS = 100;          // jeda kecil setelah init LCD

// Relay aktif HIGH (IN = HIGH -> kontak menutup -> kipas menyala).
// Kalau modul relay-mu aktif LOW, ubah nilai ini menjadi LOW.
const uint8_t RELAY_AKTIF = HIGH;
const uint8_t RELAY_NONAKTIF = LOW;

// WiFi + Telegram (isi dengan datamu; hanya dipakai bila USE_TELEGRAM 1)
const char* WIFI_SSID = "NamaWiFiKamu";
const char* WIFI_PASS = "PasswordWiFiKamu";
const char* BOT_TOKEN = "123456789:AAExxxxxxxxxxxxxxxx";
const char* CHAT_ID = "123456789";

// ---------------------------------------------------------------------------
// OBJEK GLOBAL
// ---------------------------------------------------------------------------

DHT dht(PIN_DHT, DHT22);
LiquidCrystal_I2C lcd(ALAMAT_LCD_I2C, KOLOM_LCD, BARIS_LCD);
WiFiClientSecure klienAman;
UniversalTelegramBot bot(BOT_TOKEN, klienAman);

bool kipasNyala = false;        // status kipas saat ini
bool sensorOk = true;           // false bila DHT gagal dibaca
unsigned long waktuBacaTerakhir = 0;

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("Kipas Otomatis DHT22 — mulai");

  pinMode(PIN_RELAY_KIPAS, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  digitalWrite(PIN_RELAY_KIPAS, RELAY_NONAKTIF);
  digitalWrite(PIN_LED_HIJAU, LOW);

  dht.begin();

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();
  delay(JEDA_LCD_MS);
  lcd.setCursor(0, 0);
  lcd.print("Kipas Otomatis");
  lcd.setCursor(0, 1);
  lcd.print("DHT22 siap...");

#if USE_TELEGRAM
  sambungWiFi();
  klienAman.setInsecure();  // Telegram: tanpa verifikasi sertifikat
#endif
}

// ---------------------------------------------------------------------------
// LOOP UTAMA — baca suhu tiap INTERVAL_BACA_MS, tanpa delay() yang memblokir
// ---------------------------------------------------------------------------

void loop() {
  unsigned long sekarang = millis();
  if (sekarang - waktuBacaTerakhir >= INTERVAL_BACA_MS) {
    waktuBacaTerakhir = sekarang;

    float suhu = bacaSuhu();
    kendaliKipas(suhu);
    tampilLCD(suhu);
  }
}

// ---------------------------------------------------------------------------
// SAMBUNG WIFI — berhenti mencoba setelah TIMEOUT_WIFI_MS, alat tetap jalan
// tanpa Telegram (tidak pernah hang menunggu WiFi).
// ---------------------------------------------------------------------------

#if USE_TELEGRAM
void sambungWiFi() {
  Serial.print("Menyambung WiFi ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long mulai = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - mulai < TIMEOUT_WIFI_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi tersambung, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi gagal — lanjut tanpa Telegram");
  }
}
#endif

// ---------------------------------------------------------------------------
// BACA SUHU — kembalikan NaN bila DHT gagal dibaca; pemanggil yang menangani.
// ---------------------------------------------------------------------------

float bacaSuhu() {
  float suhu = dht.readTemperature();  // Celcius
  if (isnan(suhu)) {
    Serial.println("DHT gagal dibaca (NaN)");
    return NAN;
  }
  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.println(" C");
  return suhu;
}

// ---------------------------------------------------------------------------
// KENDALI KIPAS — hysteresis: ON di SUHU_NYALA, OFF di SUHU_MATI,
// di antaranya kipas tetap pada kondisi terakhir. NaN -> kondisi terakhir
// dipertahankan (kipas tidak dipaksa mati atau nyala oleh sensor error).
// ---------------------------------------------------------------------------

void kendaliKipas(float suhu) {
  bool statusBaru = kipasNyala;

  if (isnan(suhu)) {
    sensorOk = false;
    // Kipas dibiarkan pada kondisi terakhir — jangan ubah apa pun.
    return;
  }
  sensorOk = true;

  if (suhu >= SUHU_NYALA) {
    statusBaru = true;
  } else if (suhu <= SUHU_MATI) {
    statusBaru = false;
  }

  if (statusBaru != kipasNyala) {
    kipasNyala = statusBaru;
    digitalWrite(PIN_RELAY_KIPAS, kipasNyala ? RELAY_AKTIF : RELAY_NONAKTIF);
    digitalWrite(PIN_LED_HIJAU, kipasNyala ? HIGH : LOW);
    Serial.print("Kipas ");
    Serial.println(kipasNyala ? "NYALA" : "MATI");

    // Kirim Telegram hanya saat status BERUBAH (bukan tiap pembacaan).
    kirimTelegram(kipasNyala ? "Kipas NYALA — suhu ruangan panas."
                             : "Kipas MATI — ruangan sudah sejuk.");
  }
}

// ---------------------------------------------------------------------------
// TAMPIL LCD — baris 1 suhu, baris 2 status kipas.
// Sensor error: baris 1 "Sensor error", baris 2 status kipas terakhir.
// ---------------------------------------------------------------------------

void tampilLCD(float suhu) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (!sensorOk) {
    lcd.print("Sensor error");
  } else {
    lcd.print("Suhu: ");
    lcd.print(suhu, 1);
    lcd.print(" C");
  }

  lcd.setCursor(0, 1);
  lcd.print("Kipas: ");
  lcd.print(kipasNyala ? "NYALA" : "MATI");
}

// ---------------------------------------------------------------------------
// KIRIM TELEGRAM — diam bila USE_TELEGRAM 0 atau WiFi tidak tersambung.
// ---------------------------------------------------------------------------

void kirimTelegram(const String& pesan) {
#if USE_TELEGRAM
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Telegram dilewati: WiFi tidak tersambung");
    return;
  }
  bool terkirim = bot.sendMessage(CHAT_ID, pesan, "");
  Serial.print("Telegram: ");
  Serial.println(terkirim ? "terkirim" : "GAGAL");
#else
  (void)pesan;  // tanpa Telegram, abaikan pesan
#endif
}
