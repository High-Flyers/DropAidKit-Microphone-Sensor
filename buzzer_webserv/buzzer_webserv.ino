#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "STIBDOM";
const char* password = "kacper24";

const int buzzerPin = 0;
const int ledcChannel = 0;
const int ledcChannel1 = 1;
const int resolution = 8;

IPAddress local_IP(192, 168, 0, 200); 
IPAddress gateway(192, 168, 0, 1);   
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{
    font-family: Arial;
    text-align: center;
    margin-top: 40px;
}
input[type=range]{
    width:300px;
}
</style>
</head>
<body>

<h2>BUZZER</h2>

<p>Frequency: <span id="fVal">3000</span> Hz</p>

<input type="range"
       min="0"
       max="10000"
       value="3000"
       oninput="document.getElementById('fVal').innerHTML=this.value; fetch('/update?freq='+this.value);">

</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);

    ledcAttach(ledcChannel, 2700, resolution);
    //ledcAttachPin(buzzerPin, ledcChannel);
    ledcWriteTone(ledcChannel, 2700);

    ledcAttach(ledcChannel1, 2700, resolution);
    //ledcAttachPin(buzzerPin, ledcChannel);
    ledcWriteTone(ledcChannel1, 2700);

    WiFi.config(local_IP, gateway, subnet);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    server.on("/", []() {
        server.send(200, "text/html", index_html);
    });

    server.on("/update", []() {
        if (server.hasArg("freq")) {
            int freq = server.arg("freq").toInt();
            ledcWriteTone(ledcChannel, freq);
            ledcWriteTone(ledcChannel1, freq);
        }
        server.send(200, "text/plain", "OK");
    });

    server.begin();
}

void loop() {
    server.handleClient();
}