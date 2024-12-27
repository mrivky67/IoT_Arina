#include <WiFi.h>
#include <FirebaseESP32.h>
#include <EEPROM.h>
#include "GravityTDS.h"
#include <DHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const int trigPin = 17;
const int echoPin = 5;
#define DHT_PIN 18
#define DHT_TYPE DHT22
#define pompa_nutrisiA 4    // GPIO 4
#define pompa_nutrisiB 27   // GPIO 27
#define pompa_sirkulasi 26  // GPIO 26
#define pompa_airbaku 33
#define TdsSensorPin 35

GravityTDS gravityTds;
float temperature = 29.5;
float tdsValue;
const char* ssid = "vivo 1938";
const char* password = "1234567890";
const char* prosesFuzzy = "https://fuzzy-arina-production.up.railway.app/fuzzy";
const char* getDataUrl = "https://fuzzy-arina-production.up.railway.app/get_data";
const char* getStatus = "https://fuzzy-arina-production.up.railway.app/get_status";
DHT dht(DHT_PIN, DHT_TYPE);
bool status = false;
float suhu;
float kelembaban;
float ketinggian_air;
int umur;
String tanaman;
const float max_tinggi_air = 13.5;
const float min_tinggi_air = 17.5;
// Inisialisasi Firebase
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;
// Variabel status pompa global


String statusPompaNutrisi = "OFF";
String statusPompaAirBaku = "OFF";
String statusPompaSirkulasi = "OFF";
#define FIREBASE_HOST "https://ptcarina-d9364-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyBCoRJsNxNr_9hDzVhXYwABaf8OFrk4fFo"

void getFuzzy() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Membuat JSON untuk dikirim
    StaticJsonDocument<512> doc;

    doc["umur"] = umur;
    doc["ppm"] = tdsValue;
    doc["ketinggian"] = ketinggian_air;

    String jsonString;
    serializeJson(doc, jsonString);

    // Kirim permintaan POST ke server
    http.begin(prosesFuzzy);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonString);

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Respons dari server:");
      Serial.println(payload);

      // Parsing JSON respons
      StaticJsonDocument<100> response;
      DeserializationError error = deserializeJson(response, payload);

      if (!error) {
        // Simpan status pompa ke variabel global
        statusPompaNutrisi = response["pompa_nutrisi"].as<String>();
        statusPompaAirBaku = response["pompa_airbaku"].as<String>();
        statusPompaSirkulasi = response["pompa_sirkulasi"].as<String>();
      } else {
        Serial.print("Gagal parse JSON: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.printf("HTTP Error: %d\n", httpCode);
    }

    http.end();
  } else {
    Serial.println("WiFi tidak terhubung!");
  }
}

void kontrolAktuator() {
  // Mengontrol pompa berdasarkan status global
  digitalWrite(pompa_nutrisiA, statusPompaNutrisi == "ON" ? LOW : HIGH);
  digitalWrite(pompa_nutrisiB, statusPompaNutrisi == "ON" ? LOW : HIGH);
  digitalWrite(pompa_airbaku, statusPompaAirBaku == "ON" ? LOW : HIGH);
  digitalWrite(pompa_sirkulasi, statusPompaSirkulasi == "ON" ? LOW : HIGH);

  // Menampilkan status di serial monitor
  Serial.println("Status Aktuator:");
  Serial.printf("Pompa Nutrisi: %s\n", statusPompaNutrisi.c_str());
  Serial.printf("Pompa Air Baku: %s\n", statusPompaAirBaku.c_str());
  Serial.printf("Pompa Sirkulasi: %s\n", statusPompaSirkulasi.c_str());
}

void sendStatus() {
  if (WiFi.status() == WL_CONNECTED) {  // Perbaikan: WiFi, bukan Wifi
    HTTPClient http;

    // Membuat objek JSON untuk data yang dikirim
    StaticJsonDocument<512> doc;

    doc["umur"] = umur;
    doc["ppm"] = tdsValue;
    doc["suhu"] = suhu;
    doc["kelembaban"] = kelembaban;
    doc["ketinggian_air"] = ketinggian_air;

    // Serialize JSON ke string
    String jsonString;
    serializeJson(doc, jsonString);

    // Memulai koneksi ke URL
    http.begin(getStatus);
    http.addHeader("Content-Type", "application/json");  // Menambahkan header

    // Mengirimkan data menggunakan metode POST
    int httpCode = http.POST(jsonString);

    if (httpCode > 0) {
      // Jika respons berhasil
      Serial.printf("HTTP Response Code: %d\n", httpCode);
      String payload = http.getString();
      Serial.println("Respons dari server:");
      Serial.println(payload);

      // Parsing JSON dari respons server
      StaticJsonDocument<100> response;  // Perbaikan: StaticJsonDocument, bukan StatisJsonDocument
      DeserializationError error = deserializeJson(response, payload);

      if (error) {
        Serial.print("Gagal parse JSON respons: ");
        Serial.println(error.c_str());
      } else {
        // Akses data dari JSON respons (jika ada)
        if (response.containsKey("status")) {
          const char* status = response["status"];
          Serial.printf("Status: %s\n", status);
        }
      }
    } else {
      // Jika POST gagal
      Serial.printf("Error saat POST: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();  // Mengakhiri koneksi HTTP
  } else {
    Serial.println("WiFi tidak terhubung!");
  }
}

void getData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    Serial.println("Mengirimkan GET request...");
    http.begin(getDataUrl);  // Memulai koneksi ke URL

    int httpResponseCode = http.GET();  // Melakukan GET request

    if (httpResponseCode > 0) {
      // Jika permintaan berhasil
      Serial.printf("HTTP Response Code: %d\n", httpResponseCode);
      String payload = http.getString();  // Mengambil data respons
      Serial.println("Data diterima:");
      Serial.println(payload);

      // Parsing JSON
      StaticJsonDocument<512> doc;
  // Ukuran buffer tergantung pada struktur JSON
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("Gagal parse JSON: ");
        Serial.println(error.c_str());
      } else {
        // Menyimpan nilai dari JSON ke variabel global
        tanaman = doc["tanaman"].as<String>();  // Mendapatkan data "tanaman"
        umur = doc["umur"];                     // Mendapatkan data "umur"
      }
    } else {
      // Jika permintaan gagal
      Serial.printf("Error pada GET request: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();  // Mengakhiri koneksi HTTP
  } else {
    Serial.println("WiFi tidak terhubung!");
  }
}

void initWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  Serial.println("Connected to WiFi, IP address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase() {
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Connected to Firebase");
}

float getUltrasonicDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distance = 0.017 * duration;
  ketinggian_air = distance;
  return distance;
}

void kirimFirebase() {
  // Kirim data ke Firebase
  if (Firebase.setFloat(firebaseData, "/sensor/height", ketinggian_air)) {
    Serial.println("Data tinggi dikirim ke Firebase");
  } else {
    Serial.print("Gagal mengirim data tinggi: ");
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.setFloat(firebaseData, "/sensor/tds", tdsValue)) {
    Serial.println("Data TDS dikirim ke Firebase");
  } else {
    Serial.print("Gagal mengirim data TDS: ");
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.setFloat(firebaseData, "/sensor/suhu", 27)) {
    Serial.println("Data suhu dikirim ke Firebase");
  } else {
    Serial.print("Gagal mengirim data suhu: ");
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.setFloat(firebaseData, "/sensor/kelembaban", 60)) {
    Serial.println("Data kelembaban dikirim ke Firebase");
  } else {
    Serial.print("Gagal mengirim data kelembaban: ");
    Serial.println(firebaseData.errorReason());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(pompa_nutrisiA, OUTPUT);
  pinMode(pompa_nutrisiB, OUTPUT);
  pinMode(pompa_sirkulasi, OUTPUT);
  pinMode(pompa_airbaku, OUTPUT);
  digitalWrite(pompa_nutrisiA, HIGH);
  digitalWrite(pompa_nutrisiB, HIGH);
  digitalWrite(pompa_sirkulasi, HIGH);
  digitalWrite(pompa_airbaku, HIGH);
  initWiFi();
  initFirebase();


  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(3.3);
  gravityTds.setAdcRange(4096);
  gravityTds.begin();
}

void loop() {
  // Kalibrasi TDS: memastikan nilai TDS valid sebelum melanjutkan
  if (!status) {
    // Update data TDS
    gravityTds.setTemperature(temperature);  // Atur suhu
    gravityTds.update();                     // Perbarui pembacaan sensor TDS
    tdsValue = gravityTds.getTdsValue();     // Dapatkan nilai TDS

    // Periksa apakah nilai TDS valid (lebih besar dari nol)
    if (tdsValue > 0) {
      Serial.println("Kalibrasi berhasil, nilai TDS valid!");
      status = true;  // Ubah status ke `true` untuk melanjutkan proses
    } else {
      // Tampilkan pesan jika nilai TDS belum valid
      Serial.println("Sedang melakukan kalibrasi TDS...");
    }

    delay(1000);  // Beri jeda waktu untuk kalibrasi
  } else {
    // Jika TDS sudah valid, baca dan tampilkan data sensor lainnya
    suhu = 27;
    kelembaban = 60;
    ketinggian_air = getUltrasonicDistance();
    gravityTds.setTemperature(temperature);  // Atur suhu
    gravityTds.update();                     // Perbarui pembacaan sensor TDS
    tdsValue = gravityTds.getTdsValue();
    // Periksa apakah pembacaan suhu dan kelembaban valid
    if (ketinggian_air > 0) {
      sendStatus();
      // Tampilkan data ke Serial Monitor
      Serial.print("Ketinggian air: ");
      Serial.println(ketinggian_air);
      Serial.print("PPM (TDS): ");
      Serial.println(tdsValue);
      Serial.print("Suhu: ");
      Serial.println(suhu);
      Serial.print("Kelembaban: ");
      Serial.println(kelembaban);
      getData();
      Serial.print("Umur : ");
      Serial.println(umur);
      if (umur > 0) {// Kontrol aktuator berdasarkan pembacaan sensor
        getFuzzy();
        kontrolAktuator();

        // Kirim data ke Firebase
        kirimFirebase();
      }
    } else {
      // Tampilkan pesan jika pembacaan suhu atau kelembaban gagal
      Serial.println("Gagal membaca data suhu atau kelembaban.");
    }

    delay(10000);  // Tunggu 10 detik sebelum membaca data lagi
  }
}
