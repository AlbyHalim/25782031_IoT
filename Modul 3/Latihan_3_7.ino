#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

const char* ssid = "go get em tiger";
const char* password = "albynnnnnn";

ESP8266WebServer server(80);

#define DHTPIN 2
#define DHTTYPE DHT22

const int relayPin = 12;

DHT dht(DHTPIN, DHTTYPE);

void handleRoot() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  String relayButton;

  if (digitalRead(relayPin) == LOW) {
    relayButton = "<a href=\"/relay/off\"><button>RELAY ON</button></a>";
  } else {
    relayButton = "<a href=\"/relay/on\"><button>RELAY OFF</button></a>";
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta http-equiv="refresh" content="5">
  <title>Monitoring DHT22</title>
</head>
<body>
  <h1>Monitoring Suhu dan Kelembapan</h1>

  <p>Suhu: %TEMPERATURE% &deg;C</p>
  <p>Kelembapan: %HUMIDITY% %</p>

  %RELAY_BUTTON%

</body>
</html>
)rawliteral";

  html.replace("%TEMPERATURE%", String(temperature));
  html.replace("%HUMIDITY%", String(humidity));
  html.replace("%RELAY_BUTTON%", relayButton);

  server.send(200, "text/html", html);
}

void handleRelayOn() {
  digitalWrite(relayPin, LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRelayOff() {
  digitalWrite(relayPin, HIGH);
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  dht.begin();

  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan ke WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi terhubung");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/relay/on", handleRelayOn);
  server.on("/relay/off", handleRelayOff);

  server.begin();

  Serial.println("Web Server dimulai");
}

void loop() {
  server.handleClient();
}