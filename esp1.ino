#include <DHT.h>

// ===============================
// UART
// ===============================
#define RX_PIN 16
#define TX_PIN 17
HardwareSerial UART(2);

// ===============================
// DHT22
// ===============================
#define DHT_PIN 15
#define DHT_TYPE DHT22
DHT dht(DHT_PIN,DHT_TYPE);

// ===============================
// SENSOR
// ===============================
#define MQ_PIN 34
#define WATERLEVEL_PIN 35
#define LDR_PIN 32
#define RAINDROP_PIN 33

// ===============================
// LED
// ===============================
#define LED_DAPUR 18
#define LED_TENGAH 19

// ===============================
// TIMER
// ===============================
unsigned long waktuBaca=0;
const unsigned long intervalBaca=2000;

// ===============================
// DATA SENSOR
// ===============================
float suhu=0;
float kelembapan=0;
int nilaiMQ=0;
int nilaiWaterLevel=0;
int statusLDR=0;
int nilaiRaindrop=0;

// ===============================
// SETUP
// ===============================
void setup(){
  Serial.begin(115200);
  UART.begin(9600,SERIAL_8N1,RX_PIN,TX_PIN);

  dht.begin();

  pinMode(MQ_PIN,INPUT);
  pinMode(WATERLEVEL_PIN,INPUT);
  pinMode(LDR_PIN,INPUT);
  pinMode(RAINDROP_PIN,INPUT);
  pinMode(LED_DAPUR,OUTPUT);
  pinMode(LED_TENGAH,OUTPUT);

  digitalWrite(LED_DAPUR,LOW);
  digitalWrite(LED_TENGAH,LOW);

  Serial.println();
  Serial.println("==============================");
  Serial.println("     SMART HOME FARM");
  Serial.println("==============================");
  Serial.println("ESP1 SENSOR READY");
  Serial.println("UART : 9600");
  Serial.println("TX   : GPIO17");
  Serial.println("RX   : GPIO16");
  Serial.println();
  Serial.println("DHT22       : GPIO15");
  Serial.println("MQ          : GPIO34");
  Serial.println("Water Level : GPIO35");
  Serial.println("LDR         : GPIO32");
  Serial.println("Raindrop    : GPIO33 ANALOG");
  Serial.println("==============================");
}

// ===============================
// BACA SENSOR
// ===============================
bool bacaSensor(){
  float dataSuhu=dht.readTemperature();
  float dataKelembapan=dht.readHumidity();

  if(isnan(dataSuhu)||isnan(dataKelembapan)){
    Serial.println("DHT ERROR");
    return false;
  }

  suhu=dataSuhu;
  kelembapan=dataKelembapan;

  nilaiMQ=analogRead(MQ_PIN);
  nilaiWaterLevel=analogRead(WATERLEVEL_PIN);

  // LDR tetap digital
  statusLDR=digitalRead(LDR_PIN);

  // RAINDROP ANALOG
  nilaiRaindrop=analogRead(RAINDROP_PIN);

  return true;
}

// ===============================
// TAMPIL SERIAL
// ===============================
void tampilSerial(){
  Serial.println();
  Serial.println("===== SENSOR ESP1 =====");

  Serial.print("Suhu         : ");
  Serial.print(suhu,1);
  Serial.println(" C");

  Serial.print("Kelembapan   : ");
  Serial.print(kelembapan,1);
  Serial.println(" %");

  Serial.print("MQ           : ");
  Serial.println(nilaiMQ);

  Serial.print("Water Level  : ");
  Serial.println(nilaiWaterLevel);

  Serial.print("LDR          : ");
  if(statusLDR==HIGH)
    Serial.println("TERANG");
  else
    Serial.println("GELAP");

  Serial.print("Raindrop     : ");
  Serial.println(nilaiRaindrop);

  Serial.println("=======================");
}

// ===============================
// KIRIM DATA KE ESP2
// ===============================
void kirimUART(){
  String data=
    String(suhu,1)+","+
    String(kelembapan,1)+","+
    String(nilaiMQ)+","+
    String(nilaiWaterLevel)+","+
    String(statusLDR)+","+
    String(nilaiRaindrop);

  UART.println(data);

  Serial.print("UART SEND : ");
  Serial.println(data);
}

// ===============================
// TERIMA DATA DARI ESP2
// ===============================

void bacaPerintahESP2(){
  if(UART.available()){
    String response=UART.readStringUntil('\n');
    response.trim();

    // ACK dari ESP2
    if(response=="ACK"){
      Serial.println("ACK diterima dari ESP2");
    }

    // ===========================
    // LAMPU DAPUR
    // ===========================
    else if(response=="DAPUR_ON"){
      digitalWrite(LED_DAPUR,HIGH);
      Serial.println("Lampu Dapur : ON");
    }

    else if(response=="DAPUR_OFF"){
      digitalWrite(LED_DAPUR,LOW);
      Serial.println("Lampu Dapur : OFF");
    }

    // ===========================
    // LAMPU RUANG TENGAH
    // ===========================
    else if(response=="TENGAH_ON"){
      digitalWrite(LED_TENGAH,HIGH);
      Serial.println("Lampu Ruang Tengah : ON");
    }

    else if(response=="TENGAH_OFF"){
      digitalWrite(LED_TENGAH,LOW);
      Serial.println("Lampu Ruang Tengah : OFF");
    }

    else{
      Serial.print("UART RECEIVE : ");
      Serial.println(response);
    }
  }
}

// ===============================
// LOOP
// ===============================

void loop(){
  bacaPerintahESP2();

  if(millis()-waktuBaca>=intervalBaca){
    waktuBaca=millis();

    if(bacaSensor()){
      tampilSerial();
      kirimUART();
    }
  }
}
