#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_MIC_AP";
const char* password = "hf21372137";

const int buzzerPin = 0;

// Ustawienia statycznego IP
IPAddress local_IP(192, 168, 4, 200); 
IPAddress gateway(192, 168, 4, 1);   
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h2>STEROWANIE BUZZEREM</h2>
<button style="font-size:30px; padding:20px;" onclick="fetch('/on')">WLACZ</button>
<button style="font-size:30px; padding:20px;" onclick="fetch('/off')">WYLACZ</button>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);

    // Włączenie auto-reconnect wbudowanego w ESP32
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true); // Zapisuje dane do flasha

    setupWiFi();

    server.on("/", []() { server.send(200, "text/html", index_html); });
    server.on("/on", []() { digitalWrite(buzzerPin, HIGH); server.send(200, "text/plain", "ON"); });
    server.on("/off", []() { digitalWrite(buzzerPin, LOW); server.send(200, "text/plain", "OFF"); });

    server.begin();
}

void setupWiFi() {
    WiFi.config(local_IP, gateway, subnet);
    WiFi.begin(ssid, password);
    Serial.print("Laczenie z WiFi...");
    
    // Czekamy chwilę na połączenie
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nPolaczono!");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nNie udalo sie polaczyc, probuje dalej w tle...");
    }
}

void loop() {
    // Sprawdzanie połączenia w pętli
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastCheck = 0;
        if (millis() - lastCheck > 10000) { // Sprawdzaj co 10 sekund
            Serial.println("Utracono polaczenie. Proba ponownego polaczenia...");
            WiFi.disconnect();
            WiFi.reconnect();
            lastCheck = millis();
        }
    }

    server.handleClient();
}