#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Jaringan Wi-Fi
const char* ssid = "albyhalim";
const char* password = "alby200707";

// Konfigurasi Broker MQTT Publik
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* clientId = "ESP8266_LabIoT_25782031_ClosedLoop";

// Definisi Topik Mandiri Sesuai Tugas 6.7
const char* topicPublish   = "polinela/labiot/25782031/telemetri";
const char* topicSubscribe = "polinela/labiot/25782031/kendali";
const char* topicAlert     = "polinela/labiot/25782031/alert";
const char* topicStatus    = "polinela/labiot/25782031/status";

// Alokasi Pin Perangkat Keras
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)
#define DHTTYPE DHT22  // Sesuaikan dengan sensor yang digunakan

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000; // Siklus kirim 4 detik

// Variabel flag state latch untuk mencegah spamming pesan alert
bool alertSent = false; 

// Fungsi Callback untuk menerima perintah kendali manual dari operator
void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[KONTROL DITERIMA] Topik: %s | Payload: %s\n", topic, incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);
  if (err) {
    Serial.println("Gagal membedah payload JSON!");
    return;
  }

  if (doc.containsKey("led")) {
    int val = doc["led"];
    digitalWrite(ledPin, val == 1 ? HIGH : LOW);
    Serial.printf("--> [MANUAL OVERRIDE] Status LED diubah: %s\n", val == 1 ? "MENYALA" : "PADAM");
  }
  Serial.println("----------------------------------------");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan kembali ke Broker...");
    
    // Konfigurasi Pesan Wasiat Last Will and Testament (LWT)
    // Format: client.connect(clientId, willTopic, willQos, willRetain, willMessage)
    String willMessage = "{\"status\": \"OFFLINE\", \"keterangan\": \"Perangkat Terputus Tak Terduga\"}";

    if (client.connect(clientId, topicStatus, 1, true, willMessage.c_str())) {
      Serial.println(" Sukses Terhubung!");
      
      // Daftarkan subskripsi topik kendali
      client.subscribe(topicSubscribe);
      Serial.printf("Subskripsi Topik Kendali: %s\n", topicSubscribe);

      // Publikasikan status ONLINE sesaat setelah terhubung
      String onlineMessage = "{\"status\": \"ONLINE\", \"keterangan\": \"Perangkat Beroperasi Normal\"}";
      client.publish(topicStatus, onlineMessage.c_str(), true);
    } else {
      Serial.printf("Gagal (rc=%d). Coba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Terkoneksi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Memelihara sesi TCP dan memproses callback masuk

  unsigned long now = millis();
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      // 1. Klasifikasi Kondisi Suhu (Ambang batas 31.0°C)
      String kondisiSuhu = (t > 31.0) ? "PANAS" : "NORMAL";

      // 2. Rule Engine / Safety Closed-Loop Lokal
      if (t > 31.0) {
        // Otomatis nyalakan indikator LED darurat
        digitalWrite(ledPin, HIGH);

        // Kirim alert khusus ke kanal alert jika belum terkirim (state latch)
        if (!alertSent) {
          StaticJsonDocument<256> docAlert;
          docAlert["status"] = "BAHAYA";
          docAlert["parameter"] = "OVERHEAT";
          docAlert["nilai_suhu"] = serialized(String(t, 2));

          char alertBuffer[256];
          serializeJson(docAlert, alertBuffer);
          
          if (client.publish(topicAlert, alertBuffer)) {
            Serial.printf("[ALERT TERKIRIM] %s\n", alertBuffer);
            alertSent = true; // Kunci flag agar tidak spam
          }
        }
      } else {
        // Reset flag alert jika suhu kembali normal
        if (alertSent) {
          alertSent = false;
        }
      }

      // 3. Publikasi Paket Telemetri Rutin Berformat JSON
      StaticJsonDocument<256> docOut;
      docOut["node_id"] = clientId;
      docOut["suhu"] = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["kondisi_suhu"] = kondisiSuhu;
      docOut["status_led"] = digitalRead(ledPin);

      char buffer[256];
      serializeJson(docOut, buffer);
      
      bool success = client.publish(topicPublish, buffer);
      if (success) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", buffer);
      } else {
        Serial.println("[ERROR] Gagal memublikasikan telemetri!");
      }
    }
  }
}