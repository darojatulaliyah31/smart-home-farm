/* 
==================================================
                  SMART HOME FARM
                ESP32 KEDUA / ESP2
==================================================

1. ✅Sistem Kontrol Suhu (DHT, Kipas)
2. ✅Sistem Deteksi Kebakaran (MQ2, Buzzer)
3. ✅Sistem Pintu Otomatis (RFID, Servo)
4. ✅Sistem Lampu Teras Otomatis (LDR, Lampu)
5. ✅Sistem Jemuran Otomatis (Raindrop, Servo)
6. ✅Sistem Makan Ternak Otomatis (Timer, Feeder)
7. ✅Sistem Minum Ternak Otomatis (Waterlevel, Pompa)

Fitur :
✅ Membaca RFID
✅ Menampilkan status RFID pada OLED
✅ Mengambil waktu dari internet menggunakan NTP
✅ Menampilkan waktu terkini pada OLED
✅ Terhubung ke Blynk
✅ Menjalankan jadwal makan ternak
✅ Pompa menyala dengan durasi 3 detik
✅ Servo pintu terbuka dengan durasi 5 detik

3V3 : LDR, RAINDROP, DHT22, WATERLEVEL, RFID
VIN : MQ2

*/
// ==================================================
// BLYNK
// ==================================================

#define BLYNK_TEMPLATE_ID "TMPL6hh7EgGdx"
#define BLYNK_TEMPLATE_NAME "Smart Home Farm"
#define BLYNK_AUTH_TOKEN "wQAw4A0sAsw1O_hQJjSrtjHliT0px4VN"

// ==================================================
// LIBRARY
// ==================================================

#include <time.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

Servo servoJemuran;   //OBJEK SERVO JEMURAN
Servo servoPintu;     //OBJEK SERVO PINTU
Servo servoFeeder;    //OBJEK SERVO FEEDER

// ==================================================
// WIFI
// ==================================================

char ssid[] = "PNEUMATIK 2";
char pass[] = "kanjengratu2026";


// ==================================================
// NTP / WAKTU INTERNET
// ==================================================

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;    // WIB = UTC+7
const int daylightOffset_sec = 0;       // Indonesia tidak pakai daylight saving time

// ==================================================
// UART
// ==================================================
//
// ESP1 TX GPIO17 -> ESP2 RX GPIO16
// ESP1 RX GPIO16 <- ESP2 TX GPIO17
// GND ESP1 <-> GND ESP2
// Baud rate: 9600

#define RX_PIN 16
#define TX_PIN 17

HardwareSerial UART(2);

// ==================================================
// AKTUATOR
// ==================================================

#define KIPAS         4
#define BUZZER        5
#define LAMPU_TERAS   18
#define SERVO_JEMURAN 19
#define SERVO_PINTU   14
#define FEEDER        23
#define POMPA         13
#define LAMPU_KAMAR_1 12
#define LAMPU_KAMAR_2 15
#define LAMPU_KANDANG 18

// ==================================================
// RFID MFRC522
// ==================================================

#define RFID_SS   32
#define RFID_RST  33
#define RFID_SCK  25
#define RFID_MISO 26
#define RFID_MOSI 27

MFRC522 rfid(RFID_SS, RFID_RST);

// ==================================================
// OLED
// ==================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==================================================
// BATAS SENSOR
// ==================================================

#define BATAS_SUHU        30.0
#define BATAS_ASAP        2000
#define BATAS_RAIN        2000
#define BATAS_WATER_LEVEL 2000

// POSISI SERVO JEMURAN
#define POSISI_JEMURAN_KERING 0
#define POSISI_JEMURAN_HUJAN  90

// POSISI SERVO PINTU
#define POSISI_PINTU_TUTUP 0
#define POSISI_PINTU_BUKA 90

// POSISI SERVO FEEDER
#define POSISI_FEEDER_TUTUP 0
#define POSISI_FEEDER_BUKA 90

// ==================================================
// DATA SENSOR DARI ESP1
// ==================================================

float suhu = 0;
float kelembapan = 0;
int asap = 0;
int minum = 0;
int lux = 0;
int hujan = 0;

// ==================================================
// STATUS AKTUATOR
// ==================================================

bool pinKipas = false;
bool pinBuzzer = false;
bool pinLampuTeras = false;
bool pinServoJemuran = false;
bool pinFeeder = false;
bool pinPompa = false;
bool pinLampuKamar1 = false;
bool pinLampuKamar2 = false;
bool kontrolManualKipas = false;
bool kontrolManualBuzzer = false;
bool kontrolManualLampuTeras = false;
bool kontrolManualPompa = false;
bool kontrolManualFeeder = false;
bool kontrolManualJemuran = false;
bool kontrolManualLampuKandang = false;
bool statusLampuKandang = false;

// ==================================================
// JADWAL LAMPU KANDANG
// ==================================================

const int JAM_LAMPU_KANDANG_ON = 12;
const int MENIT_LAMPU_KANDANG_ON = 33;
const int JAM_LAMPU_KANDANG_OFF = 12;
const int MENIT_LAMPU_KANDANG_OFF = 34;

// ==================================================
// TIMER POMPA WATER LEVEL
// ==================================================

bool pompaSedangMenyala = false;
unsigned long waktuMulaiPompa = 0;
const unsigned long DURASI_POMPA = 3000; // 3 detik
bool sudahDipicu = false;                // Agar pompa tidak dipicu terus-menerus

// ==================================================
// PINTU RFID
// ==================================================

bool kontrolManualPintu = false;
bool pintuTerbukaOtomatis = false;
unsigned long waktuPintuTerbuka = 0;
const unsigned long DURASI_PINTU = 5000;    // Durasi Buka Pintu tomatis

// ==================================================
// DATA RFID
// ==================================================

byte uidAyah[] = {0x25, 0xDE, 0xBE, 0x01};  // UID kartu Ayah
byte uidIbu[] = {0xDA, 0x4C, 0xC8, 0x05};   // UID kartu Ibu
String namaAkses = "";                      // Status RFID
unsigned long waktuTampil = 0;              // Variabel Waktu OLED

// ==================================================
// STATUS TAMPILAN RFID
// ==================================================
//
// false = OLED menampilkan waktu
// true  = OLED menampilkan status RFID

bool tampilRFID = false;
unsigned long waktuRFID = 0;                    // Waktu ketika kartu RFID dibaca
const unsigned long DURASI_TAMPIL_RFID = 5000;  // Status RFID ditampilkan selama 5 detik

// Durasi servo pintu terbuka
bool pintuTerbuka = false;
unsigned long waktuPintuBuka = 0;
const unsigned long DURASI_PINTU_BUKA = 5000;

// ==================================================
// JADWAL FEEDER
// ==================================================

const int JAM_MAKAN = 10;
const int MENIT_MAKAN = 28;
const int DURASI_MAKAN = 1;         // durasi buka feeder dalam detik

bool sedangMakan = false;           // Status apakah Feeder sedang aktif
unsigned long waktuMulaiMakan = 0;  // Waktu millis ketika Feeder mulai ON
int hariTerakhirMakan = -1;         // Hari terakhir jadwal dijalankan

BlynkTimer timer;                   // TIMER BLYNK

// ==================================================
// FUNGSI CEK UID
// ==================================================

bool cocokUID(byte *uidKartu, byte *uidTerdaftar, byte ukuran) {
  for(byte i = 0; i < ukuran; i++) {
    if(uidKartu[i] != uidTerdaftar[i]) {
      return false;
    }
  }

  return true;
}

// ==================================================
// FUNGSI TAMPIL WAKTU PADA SERIAL MONITOR
// ==================================================

void tampilkanWaktu() {
  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)) {
    Serial.println("Gagal mendapatkan waktu NTP");
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("          WAKTU SEKARANG");
  Serial.println("================================");

  Serial.printf(
    "Tanggal : %02d-%02d-%04d\n",
    timeinfo.tm_mday,
    timeinfo.tm_mon + 1,
    timeinfo.tm_year + 1900
  );

  Serial.printf(
    "Waktu   : %02d:%02d:%02d\n",
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  Serial.println("Zona    : WIB (UTC+7)");
  Serial.println("================================");
}

// ==================================================
// FUNGSI TAMPIL WAKTU + TEMPELKAN KARTU PADA OLED
// ==================================================

void tampilkanOLEDWaktu() {
  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // JUDUL
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Waktu Sekarang");

  // JAM
  display.setTextSize(2);
  display.setCursor(0, 18);
  display.printf(
    "%02d:%02d:%02d",
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  // PERINTAH RFID
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.println("Tempelkan Kartu");

  display.display();
}

// ==================================================
// TAMPIL STATUS RFID PADA OLED
// ==================================================

void tampilOLED(String status, String nama) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // STATUS AKSES
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(status);

  // FAMILY
  display.setCursor(0, 16);
  display.println(nama);

  display.display();
}

// ==================================================
// FUNGSI UNTUK MEMBUKA PINTU
// ==================================================

void bukaPintu() {

  // Jika sedang mode manual, RFID tidak boleh mengubah posisi pintu
  if(kontrolManualPintu) {
    Serial.println("Pintu sedang MODE MANUAL");
    Serial.println("RFID tidak mengubah posisi pintu");
    return;
  }

  // Buka pintu
  servoPintu.write(POSISI_PINTU_BUKA);

  pintuTerbukaOtomatis = true;
  waktuPintuTerbuka = millis();

  Serial.println("Pintu : TERBUKA");
  Serial.println("Mode  : OTOMATIS");
  Serial.println("Timer : 5 detik");
}
// ==================================================
// FUNGSI UNTUK MENUTUP PINTU
// ==================================================

void kontrolPintu() {
  if(pintuTerbuka && millis() - waktuPintuBuka >= DURASI_PINTU_BUKA) {
    servoPintu.write(POSISI_PINTU_TUTUP);
    pintuTerbuka = false;
    Serial.println("Pintu : TERTUTUP");
  }
}

// ==================================================
// FUNGSI KONTROL POMPA WATER LEVEL
// ==================================================

void kontrolPompa() {

  // ==================================================
  // PRIORITAS 1 : KONTROL MANUAL DARI BLYNK
  // ==================================================

  if(kontrolManualPompa) {

    digitalWrite(POMPA, LOW);
    pinPompa = true;

    // Manual ON tidak menggunakan timer
    pompaSedangMenyala = false;

    return;
  }


  // ==================================================
  // PRIORITAS 2 : KONTROL OTOMATIS
  // ==================================================

  if(minum > BATAS_WATER_LEVEL && !sudahDipicu) {

    digitalWrite(POMPA, LOW);
    pinPompa = true;
    pompaSedangMenyala = true;
    sudahDipicu = true;
    waktuMulaiPompa = millis();

    Serial.println();
    Serial.println("================================");
    Serial.println("       POMPA AKTIF");
    Serial.println("================================");
    Serial.print("Water Level : ");
    Serial.println(minum);
    Serial.println("Pompa       : ON");
    Serial.println("Mode        : OTOMATIS");
    Serial.println("Durasi      : 3 detik");
    Serial.println("================================");
  }

  // ==================================================
  // CEK DURASI POMPA OTOMATIS
  // ==================================================

  if(pompaSedangMenyala) {

    if(millis() - waktuMulaiPompa >= DURASI_POMPA) {

      digitalWrite(POMPA, HIGH);
      pinPompa = false;
      pompaSedangMenyala = false;

      Serial.println();
      Serial.println("================================");
      Serial.println("       POMPA SELESAI");
      Serial.println("================================");
      Serial.println("Pompa : OFF");
      Serial.println("================================");
    }
  }


  // ==================================================
  // RESET TRIGGER OTOMATIS
  // ==================================================

  if(minum <= BATAS_WATER_LEVEL) {
    sudahDipicu = false;
  }
}

// ==================================================
// BACA RFID
// ==================================================

void bacaRFID() {

  // CEK KARTU BARU
  if(!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // BACA UID KARTU
  if(!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("       RFID TERDETEKSI");
  Serial.println("================================");

  // TAMPIL UID
  Serial.print("UID    : ");

  for(byte i = 0; i < rfid.uid.size; i++) {
    if(rfid.uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }

    Serial.print(rfid.uid.uidByte[i], HEX);

    if(i < rfid.uid.size - 1) {
      Serial.print(" ");
    }
  }

  Serial.println();

  // ==================================================
  // CEK UID AYAH
  // ==================================================

  if(
    rfid.uid.size == sizeof(uidAyah) &&
    cocokUID(rfid.uid.uidByte, uidAyah, sizeof(uidAyah))
  ) {
    namaAkses = "AYAH";
    Serial.println("STATUS : AKSES DITERIMA");
    Serial.println("ANGGOTA: AYAH");
    tampilOLED("Akses : Diterima", "Family : Ayah");
    bukaPintu();
  }

  // ==================================================
  // CEK UID IBU
  // ==================================================

  else if(
    rfid.uid.size == sizeof(uidIbu) &&
    cocokUID(rfid.uid.uidByte, uidIbu, sizeof(uidIbu))
  ) {
    namaAkses = "IBU";
    Serial.println("STATUS : AKSES DITERIMA");
    Serial.println("ANGGOTA: IBU");
    tampilOLED("Akses : Diterima", "Family : Ibu");
    bukaPintu();
  }

  // ==================================================
  // UID TIDAK TERDAFTAR
  // ==================================================

  else {
    namaAkses = "TIDAK DIKENAL";
    Serial.println("STATUS : AKSES DITOLAK");
    Serial.println("ANGGOTA: PENYUSUP");
    tampilOLED("Akses : Ditolak", "Anggota : Penyusup");
  }

  // ==================================================
  // AKTIFKAN MODE TAMPILAN RFID
  // ==================================================

  tampilRFID = true;
  waktuRFID = millis();

  Serial.println("================================");
  Serial.println();

  // ==================================================
  // HENTIKAN KOMUNIKASI RFID
  // ==================================================

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ==================================================
// KONTROL AKTUATOR SENSOR
// ==================================================

void kontrolAktuator() {

  // ==================================================
  // KIPAS
  // ==================================================

  // PRIORITAS 1: KONTROL MANUAL DARI BLYNK
  if(kontrolManualKipas) {
    digitalWrite(KIPAS, HIGH);
    pinKipas = true;
  }

  // PRIORITAS 2: KONTROL OTOMATIS BERDASARKAN SUHU
  else {
    if(suhu >= BATAS_SUHU) {
      digitalWrite(KIPAS, HIGH);
      pinKipas = true;
    }
    else {
      digitalWrite(KIPAS, LOW);
      pinKipas = false;
    }
  }

  // ==================================================
  // BUZZER
  // ==================================================

  // PRIORITAS 1 : KONTROL MANUAL DARI BLYNK
  if(kontrolManualBuzzer) {

    tone(BUZZER, 800);
    pinBuzzer = true;

  }

  // PRIORITAS 2 : KONTROL OTOMATIS BERDASARKAN ASAP
  else {
    if(asap >= BATAS_ASAP) {
      tone(BUZZER, 800);
      pinBuzzer = true;
    }
    else {
      noTone(BUZZER);
      pinBuzzer = false;

    }

  }

// ==================================================
// LAMPU TERAS
// ==================================================

  // PRIORITAS 1 : KONTROL MANUAL DARI BLYNK
  if(kontrolManualLampuTeras) {
    digitalWrite(LAMPU_TERAS, LOW);
    pinLampuTeras = true;

  }
  // PRIORITAS 2 : KONTROL OTOMATIS BERDASARKAN LDR
  else {
    if(lux == HIGH) {
      digitalWrite(LAMPU_TERAS, LOW);
      pinLampuTeras = true;
    }
    else {
      digitalWrite(LAMPU_TERAS, HIGH);
      pinLampuTeras = false;
    }
  }

// ==================================================
// SERVO JEMURAN
// PRIORITAS MANUAL > OTOMATIS
// ==================================================

if(kontrolManualJemuran) {

  // ==================================================
  // MODE MANUAL
  // ==================================================

  // V19 ON = POSISI HUJAN / TERTUTUP
  servoJemuran.write(POSISI_JEMURAN_HUJAN);
  pinServoJemuran = true;

}
else {

  // ==================================================
  // MODE OTOMATIS
  // ==================================================

  if(hujan < BATAS_RAIN) {

    // HUJAN → TUTUP
    servoJemuran.write(POSISI_JEMURAN_HUJAN);
    pinServoJemuran = true;

    Serial.println("Jemuran : TERTUTUP");
    Serial.println("Mode    : OTOMATIS");
    Serial.println("Alasan  : HUJAN");

  }
  else {

    // KERING → BUKA
    servoJemuran.write(POSISI_JEMURAN_KERING);
    pinServoJemuran = false;

    Serial.println("Jemuran : TERBUKA");
    Serial.println("Mode    : OTOMATIS");
    Serial.println("Alasan  : KERING");
  }
}

  tampilSerial();
}

// ==================================================
// BACA DATA UART DARI ESP1
// ==================================================

void bacaUART() {

  if(UART.available()) {

    // ==================================================
    // BACA DATA
    // ==================================================

    String data = UART.readStringUntil('\n');
    data.trim();
    Serial.print("Data UART : ");
    Serial.println(data);

    // ==================================================
    // CARI POSISI KOMA
    // ==================================================

    int index1 = data.indexOf(',');
    int index2 = data.indexOf(',', index1 + 1);
    int index3 = data.indexOf(',', index2 + 1);
    int index4 = data.indexOf(',', index3 + 1);
    int index5 = data.indexOf(',', index4 + 1);

    // ==================================================
    // VALIDASI FORMAT
    // ==================================================

    if(
      index1 > 0 &&
      index2 > 0 &&
      index3 > 0 &&
      index4 > 0 &&
      index5 > 0
    ) {


      suhu = data.substring(0, index1).toFloat();                     // DATA SUHU
      kelembapan = data.substring(index1 + 1, index2).toFloat();      // DATA KELEMBAPAN
      asap = data.substring(index2 + 1, index3).toInt();              // DATA MQ
      minum = data.substring(index3 + 1, index4).toInt();             // DATA WATER LEVEL
      lux = data.substring(index4 + 1, index5).toInt();               // DATA LDR
      hujan = data.substring(index5 + 1).toInt();                     // DATA RAINDROP

      Serial.println("Data UART VALID");
      kontrolAktuator();                                              // KONTROL AKTUATOR

      // ==================================================
      // KIRIM ACK KE ESP1
      // ==================================================

      UART.println("ACK");
      Serial.println("ACK dikirim");
      Serial.println("------------------------------");
    } else {
      Serial.println("Data UART CORRUPT / FORMAT SALAH");
    }
  }
}


// ==================================================
// TAMPIL DATA SENSOR PADA SERIAL
// ==================================================

void tampilSerial() {

  Serial.println();
  Serial.println("===== DATA ESP1 =====");

  // SUHU
  Serial.print("Suhu         : ");
  Serial.print(suhu);
  Serial.println(" C");

  // KELEMBAPAN
  Serial.print("Kelembapan   : ");
  Serial.print(kelembapan);
  Serial.println(" %");

  // MQ
  Serial.print("Asap         : ");
  Serial.println(asap);

  // WATER LEVEL
  Serial.print("Water Level  : ");
  Serial.println(minum);

  // LDR
  Serial.print("LDR          : ");
  if(lux == HIGH) {
    Serial.println("TERANG");
  } else {
    Serial.println("GELAP");
  }

  // RAINDROP
  Serial.print("Raindrop     : ");
  Serial.println(hujan);
  Serial.print("Status Hujan : ");
  if(hujan < BATAS_RAIN) {
    Serial.println("HUJAN");
  } else {
    Serial.println("KERING");
  }

  // ==================================================
  // AKTUATOR
  // ==================================================

  Serial.println();
  Serial.println("===== AKTUATOR =====");

  // Kipas
  Serial.print("Kipas            : ");
  if(pinKipas) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  // Buzzer
  Serial.print("Buzzer           : ");
  if(pinBuzzer) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  // Lampu Teras
  Serial.print("Lampu Teras      : ");
  if(pinLampuTeras) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  // Servo Jemuran
  Serial.print("Servo Jemuran    : ");
  if(pinServoJemuran) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  // Feeder
  Serial.print("Feeder           : ");
  if(pinFeeder) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  Serial.println("====================");
}

// ==================================================
// KONTROL JADWAL MAKAN SERVO FEEDER
// ==================================================

void kontrolJadwalMakan() {

  // ==================================================
  // PRIORITAS 1 : KONTROL MANUAL DARI BLYNK
  // ==================================================

  if(kontrolManualFeeder) {

    // Manual ON = buka feeder 90 derajat
    servoFeeder.write(POSISI_FEEDER_BUKA);
    pinFeeder = true;

    // Matikan status otomatis
    sedangMakan = false;

    return;
  }


  // ==================================================
  // PRIORITAS 2 : KONTROL OTOMATIS
  // ==================================================

  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)) {
    return;
  }

  int jamSekarang = timeinfo.tm_hour;
  int menitSekarang = timeinfo.tm_min;
  int detikSekarang = timeinfo.tm_sec;
  int hariSekarang = timeinfo.tm_yday;


  // ==================================================
  // CEK WAKTU MULAI MAKAN
  // ==================================================

  if(
    jamSekarang == JAM_MAKAN &&
    menitSekarang == MENIT_MAKAN &&
    !sedangMakan &&
    hariTerakhirMakan != hariSekarang
  ) {

    // Buka feeder
    servoFeeder.write(POSISI_FEEDER_BUKA);

    pinFeeder = true;
    sedangMakan = true;
    waktuMulaiMakan = millis();
    hariTerakhirMakan = hariSekarang;

    Serial.println();
    Serial.println("================================");
    Serial.println("       JADWAL MAKAN AKTIF");
    Serial.println("================================");

    Serial.printf(
      "Waktu mulai : %02d:%02d:%02d\n",
      jamSekarang,
      menitSekarang,
      detikSekarang
    );

    Serial.print("Servo       : BUKA ");
    Serial.print(POSISI_FEEDER_BUKA);
    Serial.println(" derajat");

    Serial.print("Durasi      : ");
    Serial.print(DURASI_MAKAN);
    Serial.println(" menit");

    Serial.println("Mode        : OTOMATIS");
    Serial.println("================================");
  }


  // ==================================================
  // CEK DURASI MAKAN
  // ==================================================

  if(sedangMakan) {

    // DURASI_MAKAN dalam menit
    unsigned long durasiMillis =
      DURASI_MAKAN * 1000UL;

    if(millis() - waktuMulaiMakan >= durasiMillis) {

      // Tutup feeder
      servoFeeder.write(POSISI_FEEDER_TUTUP);

      pinFeeder = false;
      sedangMakan = false;

      Serial.println();
      Serial.println("================================");
      Serial.println("       JADWAL MAKAN SELESAI");
      Serial.println("================================");

      Serial.print("Servo       : TUTUP ");
      Serial.print(POSISI_FEEDER_TUTUP);
      Serial.println(" derajat");

      Serial.println("Mode        : OTOMATIS");
      Serial.println("================================");
    }
  }
}

// ==================================================
// KIRIM DATA KE BLYNK
// ==================================================

void kirimKeBlynk() {

  // ==================================================
  // DATA SENSOR
  // ==================================================

  Blynk.virtualWrite(V0, suhu);
  Blynk.virtualWrite(V1, kelembapan);
  Blynk.virtualWrite(V2, asap);
  Blynk.virtualWrite(V3, minum);
  Blynk.virtualWrite(V4, lux);
  Blynk.virtualWrite(V5, hujan);

  // ==================================================
  // STATUS AKTUATOR
  // ==================================================

  Blynk.virtualWrite(V6, pinKipas);
  Blynk.virtualWrite(V7, pinBuzzer);
  Blynk.virtualWrite(V8, pinLampuTeras);
  Blynk.virtualWrite(V9, pinServoJemuran);
  Blynk.virtualWrite(V10, pinPompa);
  Blynk.virtualWrite(V11, pinLampuKamar1);
  Blynk.virtualWrite(V12, pinLampuKamar2);
  Blynk.virtualWrite(V13, pinFeeder);
  Blynk.virtualWrite(V14, kontrolManualKipas);
  Blynk.virtualWrite(V15, kontrolManualBuzzer);
  Blynk.virtualWrite(V16, kontrolManualLampuTeras);
  Blynk.virtualWrite(V17, kontrolManualPompa);
  Blynk.virtualWrite(V18, kontrolManualFeeder);

    // STATUS ASAP
  if(asap >= BATAS_ASAP) {
    Blynk.virtualWrite(V22, "BAHAYA");
  }
  else {
    Blynk.virtualWrite(V22, "AMAN");
  }

    // STATUS HUJAN
  if(hujan >= BATAS_RAIN) {
    Blynk.virtualWrite(V23, "HUJAN");
  }
  else {
    Blynk.virtualWrite(V23, "TIDAK HUJAN");
  }

  // STATUS MINUM
  if(minum >= BATAS_WATER_LEVEL) {
    Blynk.virtualWrite(V24, "AIR HABIS");
  }
  else {
    Blynk.virtualWrite(V24, "AIR CUKUP");
  }
}

// ==================================================
// KONTROL LAMPU KAMAR 1 DARI BLYNK
// ==================================================

BLYNK_WRITE(V11) {

  int status = param.asInt();
  if(status == 1) {
    digitalWrite(LAMPU_KAMAR_1, HIGH);
    pinLampuKamar1 = true;
    Serial.println("Lampu Kamar 1 : ON");
  } 
  else {
    digitalWrite(LAMPU_KAMAR_1, LOW);
    pinLampuKamar1 = false;
    Serial.println("Lampu Kamar 1 : OFF");
  }
}

// ==================================================
// KONTROL LAMPU KAMAR 2 DARI BLYNK
// ==================================================

BLYNK_WRITE(V12) {
  int status = param.asInt();
  if(status == 1) {
    digitalWrite(LAMPU_KAMAR_2, HIGH);
    pinLampuKamar2 = true;
    Serial.println("Lampu Kamar 2 : ON");
  } 
  else {
    digitalWrite(LAMPU_KAMAR_2, LOW);
    pinLampuKamar2 = false;
    Serial.println("Lampu Kamar 2 : OFF");
  }
}

// ==================================================
// KONTROL MANUAL KIPAS DARI BLYNK
// ==================================================

BLYNK_WRITE(V14) {

  int status = param.asInt();
  if(status == 1) {
    kontrolManualKipas = true;                                 // Aktifkan mode manual
    digitalWrite(KIPAS, HIGH);                              // Paksa kipas ON
    pinKipas = true;
    Serial.println("================================");
    Serial.println("KIPAS MANUAL : ON");
    Serial.println("Mode         : MANUAL");
    Serial.println("================================");

  }
  else {

    kontrolManualKipas = false;           // Matikan mode manual
    if(suhu >= BATAS_SUHU) {           // Setelah manual OFF, kembalikan kontrol ke sistem otomatis
      digitalWrite(KIPAS, HIGH);              
      pinKipas = true;
      Serial.println("KIPAS : ON");
      Serial.println("Mode  : OTOMATIS");
      Serial.println("Alasan: Suhu >= batas");
    }
    else {
      digitalWrite(KIPAS, LOW);
      pinKipas = false;
      Serial.println("KIPAS : OFF");
      Serial.println("Mode  : OTOMATIS");
      Serial.println("Alasan: Suhu < batas");
    }
  }
}

// ==================================================
// KONTROL MANUAL BUZZER DARI BLYNK
// ==================================================

BLYNK_WRITE(V15) {

  int status = param.asInt();

  // MANUAL ON
  if(status == 1) {
    kontrolManualBuzzer = true;
    tone(BUZZER, 800);
    pinBuzzer = true;
    Serial.println("================================");
    Serial.println("BUZZER MANUAL : ON");
    Serial.println("Mode          : MANUAL");
    Serial.println("================================");
  }

  // MANUAL OFF
  else {
    kontrolManualBuzzer = false;

    // Setelah manual OFF, kontrol kembali ke sistem otomatis
    if(asap >= BATAS_ASAP) {         
      tone(BUZZER, 800);
      pinBuzzer = true;
      Serial.println("BUZZER : ON");
      Serial.println("Mode   : OTOMATIS");
      Serial.println("Alasan : Asap >= batas");
    }
    else {
      noTone(BUZZER);
      pinBuzzer = false;
      Serial.println("BUZZER : OFF");
      Serial.println("Mode   : OTOMATIS");
      Serial.println("Alasan : Asap < batas");

    }

  }
}

// ==================================================
// KONTROL MANUAL LAMPU TERAS DARI BLYNK
// ==================================================

BLYNK_WRITE(V16) {
  int status = param.asInt();

  // MANUAL ON
  if(status == 1) {
    kontrolManualLampuTeras = true;
    digitalWrite(LAMPU_TERAS, LOW);
    pinLampuTeras = true;
    Serial.println("================================");
    Serial.println("LAMPU TERAS MANUAL : ON");
    Serial.println("Mode               : MANUAL");
    Serial.println("================================");
  }

  // MANUAL OFF
  else {
    kontrolManualLampuTeras = false;    // Setelah manual OFF,kontrol kembali ke sistem otomatis
    if(lux == LOW) {
      digitalWrite(LAMPU_TERAS, LOW);
      pinLampuTeras = true;
      Serial.println("LAMPU TERAS : ON");
      Serial.println("Mode        : OTOMATIS");
      Serial.println("Alasan      : Kondisi GELAP");
    }
    else {
      digitalWrite(LAMPU_TERAS, HIGH);
      pinLampuTeras = false;
      Serial.println("LAMPU TERAS : OFF");
      Serial.println("Mode        : OTOMATIS");
      Serial.println("Alasan      : Kondisi TERANG");
    }
  }
}

// ==================================================
// KONTROL MANUAL POMPA DARI BLYNK
// ==================================================

BLYNK_WRITE(V17) {

  int status = param.asInt();


  // ==================================================
  // MANUAL ON
  // ==================================================

  if(status == 1) {

    kontrolManualPompa = true;

    // Pompa ON terus selama switch ON
    digitalWrite(POMPA, LOW);
    pinPompa = true;

    // Matikan status timer otomatis
    pompaSedangMenyala = false;

    Serial.println("================================");
    Serial.println("POMPA MANUAL : ON");
    Serial.println("Mode         : MANUAL");
    Serial.println("Durasi       : TERUS MENYALA");
    Serial.println("================================");

  }


  // ==================================================
  // MANUAL OFF
  // ==================================================

  else {

    kontrolManualPompa = false;

    // Pompa langsung OFF ketika switch OFF
    digitalWrite(POMPA, HIGH);
    pinPompa = false;

    // Pastikan timer otomatis tidak sedang berjalan
    pompaSedangMenyala = false;

    Serial.println("================================");
    Serial.println("POMPA MANUAL : OFF");
    Serial.println("Pompa        : OFF");
    Serial.println("Mode         : OTOMATIS");
    Serial.println("================================");

  }
}

// ==================================================
// KONTROL MANUAL SERVO FEEDER DARI BLYNK
// V18
// ==================================================

BLYNK_WRITE(V18) {

  int status = param.asInt();


  // ==================================================
  // MANUAL ON
  // ==================================================

  if(status == 1) {

    kontrolManualFeeder = true;

    // Batalkan proses otomatis yang sedang berjalan
    sedangMakan = false;

    // Buka feeder 90 derajat
    servoFeeder.write(POSISI_FEEDER_BUKA);

    pinFeeder = true;

    Serial.println("================================");
    Serial.println("FEEDER MANUAL : ON");
    Serial.println("Servo         : BUKA 90 derajat");
    Serial.println("Mode          : MANUAL");
    Serial.println("================================");
  }


  // ==================================================
  // MANUAL OFF
  // KEMBALI KE OTOMATIS
  // ==================================================

  else {

    kontrolManualFeeder = false;

    // Tutup feeder
    servoFeeder.write(POSISI_FEEDER_TUTUP);

    pinFeeder = false;

    sedangMakan = false;

    Serial.println("================================");
    Serial.println("FEEDER MANUAL : OFF");
    Serial.println("Servo         : TUTUP 0 derajat");
    Serial.println("Mode          : OTOMATIS");
    Serial.println("================================");
  }
}

// ==================================================
// KONTROL MANUAL SERVO JEMURAN DARI BLYNK
// V19
// ==================================================

BLYNK_WRITE(V19) {

  int status = param.asInt();

  // ==================================================
  // V19 ON → MODE MANUAL
  // ==================================================

  if(status == 1) {

    kontrolManualJemuran = true;

    // Manual ON = posisi hujan / tertutup
    servoJemuran.write(POSISI_JEMURAN_HUJAN);
    pinServoJemuran = true;

    Serial.println("================================");
    Serial.println("JEMURAN MANUAL : ON");
    Serial.println("Posisi         : HUJAN / TUTUP");
    Serial.println("Mode           : MANUAL");
    Serial.println("================================");

  }

  // ==================================================
  // V19 OFF → KEMBALI KE OTOMATIS
  // ==================================================

  else {

    kontrolManualJemuran = false;

    Serial.println("================================");
    Serial.println("JEMURAN MANUAL : OFF");
    Serial.println("Mode           : OTOMATIS");
    Serial.println("================================");

    // Langsung kembali mengikuti sensor hujan
    if(hujan < BATAS_RAIN) {

      servoJemuran.write(POSISI_JEMURAN_HUJAN);
      pinServoJemuran = true;

      Serial.println("Jemuran : TERTUTUP");
      Serial.println("Alasan  : HUJAN");

    }
    else {

      servoJemuran.write(POSISI_JEMURAN_KERING);
      pinServoJemuran = false;

      Serial.println("Jemuran : TERBUKA");
      Serial.println("Alasan  : KERING");
    }
  }
}

BLYNK_WRITE(V20) {

  int status = param.asInt();

  if(status == 1) {

    // =================================
    // MANUAL ON
    // =================================

    kontrolManualPintu = true;

    // Batalkan proses otomatis RFID
    pintuTerbukaOtomatis = false;

    // Buka pintu
    servoPintu.write(POSISI_PINTU_BUKA);

    Serial.println("================================");
    Serial.println("PINTU MANUAL : ON");
    Serial.println("Posisi       : TERBUKA");
    Serial.println("Mode         : MANUAL");
    Serial.println("================================");

  }
  else {

    // =================================
    // MANUAL OFF
    // KEMBALI KE OTOMATIS
    // =================================

    kontrolManualPintu = false;

    // Tutup pintu
    servoPintu.write(POSISI_PINTU_TUTUP);

    pintuTerbukaOtomatis = false;

    Serial.println("================================");
    Serial.println("PINTU MANUAL : OFF");
    Serial.println("Posisi       : TERTUTUP");
    Serial.println("Mode         : OTOMATIS");
    Serial.println("================================");
  }
}

BLYNK_WRITE(V21) {

  int status = param.asInt();

  if(status == 1) {

    // =================================
    // MANUAL ON
    // =================================

    kontrolManualLampuKandang = true;

    digitalWrite(LAMPU_KANDANG, LOW);

    statusLampuKandang = true;

    Serial.println("================================");
    Serial.println("LAMPU KANDANG : MANUAL ON");
    Serial.println("Status        : MENYALA");
    Serial.println("Mode          : MANUAL");
    Serial.println("================================");

  }
  else {

    // =================================
    // MANUAL OFF
    // KEMBALI KE OTOMATIS
    // =================================

    kontrolManualLampuKandang = false;

    Serial.println("================================");
    Serial.println("LAMPU KANDANG : MANUAL OFF");
    Serial.println("Mode          : OTOMATIS");
    Serial.println("================================");

    // Langsung cek jadwal saat ini
    kontrolLampuKandang();
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("==============================");
  Serial.println("     SMART HOME FARM");
  Serial.println("        ESP32 KEDUA");
  Serial.println("==============================");

  // ==================================================
  // WIFI
  // ==================================================

  Serial.println("Menghubungkan WiFi...");
  WiFi.begin(ssid, pass);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi TERHUBUNG");
  Serial.print("IP ESP2 : ");
  Serial.println(WiFi.localIP());

  // ==================================================
  // NTP
  // ==================================================

  Serial.println();
  Serial.println("Mengambil waktu dari internet...");
  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );

  // ==================================================
  // TUNGGU SINKRONISASI WAKTU
  // ==================================================

  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)) {
    Serial.println("Menunggu sinkronisasi waktu...");
    delay(1000);
  }
  Serial.println("Waktu berhasil disinkronisasi.");
  tampilkanWaktu();                                         // Tampilkan waktu ke Serial

  // ==================================================
  // BLYNK
  // ==================================================

  Serial.println();
  Serial.println("Menghubungkan Blynk...");
  Blynk.config(BLYNK_AUTH_TOKEN);
  while(!Blynk.connected()) {
    Blynk.connect();
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Blynk TERHUBUNG");

  // UART
  UART.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  // INISIALISASI SERVO JEMURAN
  servoJemuran.attach(SERVO_JEMURAN, 500, 2400);
  servoJemuran.write(POSISI_JEMURAN_KERING);        // Posisi awal: jemuran terbuka

  // INISIALISASI SERVO PINTU
  servoPintu.attach(SERVO_PINTU, 500, 2400);
  servoPintu.write(POSISI_PINTU_TUTUP);             // Posisi awal: pintu tertutup

  // INISIALISASI SERVO FEEDER
  servoFeeder.attach(FEEDER, 500, 2400);
  servoFeeder.write(POSISI_FEEDER_TUTUP);

  // ==================================================
  // PIN AKTUATOR
  // ==================================================

  pinMode(KIPAS, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LAMPU_TERAS, OUTPUT);
  pinMode(POMPA, OUTPUT);
  pinMode(LAMPU_KAMAR_1, OUTPUT);
  pinMode(LAMPU_KAMAR_2, OUTPUT);
  pinMode(LAMPU_KANDANG, OUTPUT);

digitalWrite(LAMPU_KANDANG, HIGH);
statusLampuKandang = false;

  // ==================================================
  // SEMUA AKTUATOR OFF
  // ==================================================

  digitalWrite(KIPAS, LOW);
  noTone(BUZZER);
  digitalWrite(LAMPU_TERAS, LOW);
  digitalWrite(POMPA, LOW);
  digitalWrite(LAMPU_KAMAR_1, LOW);
  digitalWrite(LAMPU_KAMAR_2, LOW);

  // ==================================================
  // OLED
  // ==================================================

  Wire.begin(21, 22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED gagal");
    while(1);
  }
  display.clearDisplay();
  display.display();

  // ==================================================
  // RFID
  // ==================================================

  SPI.begin(
    RFID_SCK,
    RFID_MISO,
    RFID_MOSI,
    RFID_SS
  );
  rfid.PCD_Init();
  delay(100);

  // TIMER BLYNK
  timer.setInterval(2000L, kirimKeBlynk);

  // OLED AWAL
  tampilkanOLEDWaktu();

  // ==================================================
  // ESP2 READY
  // ==================================================

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP2 READY");
  Serial.println("WiFi  : OK");
  Serial.println("NTP   : OK");
  Serial.println("Blynk : OK");
  Serial.println("UART  : OK");
  Serial.println("OLED  : OK");
  Serial.println("RFID  : OK");

  // Tampilkan jadwal secara otomatis
  Serial.printf(
    "Makan : %02d:%02d / %d menit\n",
    JAM_MAKAN,
    MENIT_MAKAN,
    DURASI_MAKAN
  );
  Serial.println("==============================");
  Serial.println();
}

//  ============================================
//  PINTU OTOMATIS
//  ============================================

void kontrolPintuOtomatis() {

  // Jika sedang mode manual, otomatis tidak boleh mengontrol servo
  if(kontrolManualPintu) {
    return;
  }

  // Jika pintu sedang dibuka oleh RFID
  if(pintuTerbukaOtomatis) {

    if(millis() - waktuPintuTerbuka >= DURASI_PINTU) {

      servoPintu.write(POSISI_PINTU_TUTUP);

      pintuTerbukaOtomatis = false;

      Serial.println("Pintu : TERTUTUP");
      Serial.println("Mode  : OTOMATIS");
    }
  }
}

void kontrolLampuKandang() {

  // Jika manual aktif, otomatis tidak bekerja
  if(kontrolManualLampuKandang) {
    return;
  }

  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)) {
    Serial.println("Gagal mendapatkan waktu NTP");
    return;
  }

  int jam = timeinfo.tm_hour;
  int menit = timeinfo.tm_min;

  int waktuSekarang = jam * 60 + menit;

  int waktuON =
    JAM_LAMPU_KANDANG_ON * 60 +
    MENIT_LAMPU_KANDANG_ON;

  int waktuOFF =
    JAM_LAMPU_KANDANG_OFF * 60 +
    MENIT_LAMPU_KANDANG_OFF;

  bool seharusnyaON;

  // Jadwal melewati tengah malam
  if(waktuON > waktuOFF) {

    seharusnyaON =
      (waktuSekarang >= waktuON ||
       waktuSekarang < waktuOFF);

  }
  else {

    seharusnyaON =
      (waktuSekarang >= waktuON &&
       waktuSekarang < waktuOFF);
  }

  // Hanya ubah output jika status berubah
  if(seharusnyaON != statusLampuKandang) {

    if(seharusnyaON) {
      digitalWrite(LAMPU_KANDANG, LOW);
      statusLampuKandang = true;

      Serial.println("Lampu Kandang : ON");
      Serial.println("Mode           : OTOMATIS");
    }
    else {
      digitalWrite(LAMPU_KANDANG, HIGH);
      statusLampuKandang = false;

      Serial.println("Lampu Kandang : OFF");
      Serial.println("Mode           : OTOMATIS");
    }
  }
}

// ==================================================
// LOOP
// ==================================================

void loop() {

  Blynk.run();            // BLYNK
  timer.run();            // TIMER BLYNK
  bacaUART();             // BACA UART
  bacaRFID();             // BACA RFID
  kontrolJadwalMakan();   // KONTROL JADWAL MAKAN
  kontrolPintu();         // KONTROL PINTU
  kontrolPompa();         // KONTROL POMPA
  kontrolPintuOtomatis();
  kontrolLampuKandang();

  // ==================================================
  // UPDATE OLED SETIAP 1 DETIK
  // ==================================================

  if(millis() - waktuTampil >= 1000) {
    waktuTampil = millis();

    // JIKA SEDANG MENAMPILKAN RFID
    if(tampilRFID) {
      if(millis() - waktuRFID >= DURASI_TAMPIL_RFID) {          // Cek apakah sudah 5 detik
        tampilRFID = false;
        tampilkanOLEDWaktu();                                   // Kembali ke tampilan normal
      }

    // ==================================================
    // JIKA TIDAK ADA RFID
    // ==================================================

    } else {
      tampilkanOLEDWaktu();
    }
  }
}
