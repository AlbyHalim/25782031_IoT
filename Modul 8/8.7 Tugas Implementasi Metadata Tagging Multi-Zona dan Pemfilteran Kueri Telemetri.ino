/*
 * ==============================================================================
 * TUGAS 8.7: IMPLEMENTASI METADATA TAGGING MULTI-ZONA DAN TELEMETRI SENSOR
 * ==============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. Kredensial Jaringan Wi-Fi (Sesuaikan dengan SSID & Password Hotspot Anda)
const char* ssid     = "albyh";
const char* password = "albyhalim";

// 2. Konfigurasi Broker MQTT Lokal
// Ganti dengan Alamat IPv4 Laptop Anda (cek melalui perintah 'ipconfig')
const char* mqttServer = "10.152.104.69";
const int   mqttPort   = 1883;

// Kredensial Autentikasi Broker MQTT
const char* mqttUser   = "admin";
const char* mqttPass   = "alby2007";

// 3. Konfigurasi Identitas Node & Topik MQTT
const char* clientId      = "ESP8266_Local_Node01";
const char* topicPubTele  = "polinela/labiot/telemetri";
const char* topicSubCmd   = "polinela/labiot/kendali";

// 4. Konfigurasi Alokasi Pin Perangkat Keras
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)

#define DHTTYPE DHT22  // Menggunakan sensor DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Variabel Pewaktu Asinkron (Non-Blocking Timer)
unsigned long lastTeleTime = 0;
const unsigned long teleInterval = 4000; // Kirim data telemetri setiap 4 detik

// Penanganan Paket Masuk (Callback Kendali Aktuator LED)
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("\n----------------------------------------");
  Serial.printf("[KENDALI MASUK] Topik: %s\n", topic);
  Serial.printf("[PAYLOAD]       : %s\n", incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.printf("[ERROR] Gagal parsing JSON instruksi: %s\n", err.c_str());
    return;
  }

  if (doc.containsKey("led")) {
    int stateLED = doc["led"];
    digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);
    Serial.printf("--> [AKTUATOR] Status LED diubah menjadi: %s\n", stateLED == 1 ? "MENYALA" : "PADAM");
  }
  Serial.println("----------------------------------------");
}

void connectToWiFi() {
  Serial.println();
  Serial.print("[Wi-Fi] Menghubungkan ke ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Wi-Fi] Berhasil Terhubung!");
  Serial.print("[Wi-Fi] IP NodeMCU: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("[MQTT] Menghubungkan ke Broker Lokal (");
    Serial.print(mqttServer);
    Serial.print(")...");

    if (client.connect(clientId, mqttUser, mqttPass)) {
      Serial.println(" Sukses Terhubung!");
      client.subscribe(topicSubCmd);
      Serial.printf("[MQTT] Berlangganan topik kendali: %s\n", topicSubCmd);
    } else {
      Serial.printf(" Gagal (rc=%d). Mencoba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Inisialisasi awal: LED padam

  dht.begin();
  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); 

  // Pengiriman Telemetri Periodik Non-Blocking
  unsigned long now = millis();
  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      // Menyusun dokumen JSON telemetri dengan pengayaan Multi-Zona
      StaticJsonDocument<256> docOut;
      docOut["node_id"]    = clientId;
      docOut["suhu"]       = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);
      docOut["zona"]       = "Zona-A"; // Metadata penandaan zona ruang sensor

      char teleBuffer[256];
      serializeJson(docOut, teleBuffer);

      bool success = client.publish(topicPubTele, teleBuffer);
      if (success) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", teleBuffer);
      } else {
        Serial.println("[ERROR] Gagal memublikasikan telemetri ke broker!");
      }
    } else {
      Serial.println("[ERROR] Gagal membaca data dari sensor fisik DHT22!");
    }
  }
}