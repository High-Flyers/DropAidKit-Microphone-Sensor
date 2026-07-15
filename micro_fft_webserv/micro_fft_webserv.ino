#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

IPAddress local_IP(192, 168, 0, 200); 
IPAddress gateway(192, 168, 0, 1);   
IPAddress subnet(255, 255, 255, 0);

// --- KONFIGURACJA ---
const char* ssid = "ESP32_MIC_AP";
const char* password = "hf21372137";
#define BUZZER_PIN 2
#define I2S_WS 7
#define I2S_SD 5
#define I2S_SCK 6
#define SAMPLES 256
#define SAMPLING_FREQ 16000

// --- ZMIENNE ---
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, (double)SAMPLING_FREQ);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- HTML / JS INTERFEJS (bez zadnych zewnetrznych bibliotek) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<meta charset="UTF-8">
<style>
  body { font-family: Arial; text-align: center; background:#111; color:#eee; margin:0; padding:10px; }
  canvas { background:#000; border:1px solid #444; width:100%; max-width:800px; height:400px; }
</style>
</head><body>
<h2>Rozklad FFT</h2>
<canvas id="fftChart" width="800" height="400"></canvas>
<script>
  var canvas = document.getElementById('fftChart');
  var ctx = canvas.getContext('2d');
  var W = canvas.width, H = canvas.height;
  var barCount = 128;
  var freqStep = 62.5; // SAMPLING_FREQ / SAMPLES = 16000/256

  var marginLeft = 45;  // miejsce na etykiety osi Y
  var marginBottom = 20; // miejsce na etykiety osi X
  var plotW = W - marginLeft;
  var plotH = H - marginBottom;

  function drawChart(data) {
    ctx.clearRect(0, 0, W, H);

    var maxVal = Math.max(...data, 1000);
    var barWidth = plotW / barCount;

    // --- Siatka + etykiety osi Y ---
    var ySteps = 5;
    ctx.strokeStyle = '#333';
    ctx.fillStyle = '#888';
    ctx.font = '10px Arial';
    ctx.textAlign = 'right';

    for (var s = 0; s <= ySteps; s++) {
      var val = Math.round((maxVal / ySteps) * s);
      var y = plotH - (plotH * s / ySteps);

      // linia pozioma siatki
      ctx.beginPath();
      ctx.moveTo(marginLeft, y);
      ctx.lineTo(W, y);
      ctx.stroke();

      // etykieta liczbowa
      ctx.fillText(val, marginLeft - 5, y + 3);
    }

    // --- Slupki FFT ---
    for (var i = 0; i < data.length; i++) {
      var val = data[i];
      var barHeight = (val / maxVal) * plotH;
      var x = marginLeft + i * barWidth;
      var y = plotH - barHeight;

      ctx.fillStyle = '#4da3ff';
      ctx.fillRect(x, y, barWidth - 1, barHeight);
    }

    // --- Etykiety osi X (Hz) ---
    ctx.fillStyle = '#888';
    ctx.textAlign = 'left';
    for (var i = 0; i < data.length; i += 16) {
      var freq = Math.round(i * freqStep);
      ctx.fillText(freq + 'Hz', marginLeft + i * barWidth, H - 5);
    }

    // --- Podpis osi Y ---
    ctx.save();
    ctx.translate(12, plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = 'center';
    ctx.fillText('Amplituda', 0, 0);
    ctx.restore();
  }

  var ws = new WebSocket(`ws://${window.location.hostname}/ws`);
  ws.onopen = function() { console.log('WS polaczony'); };
  ws.onerror = function(e) { console.log('WS blad', e); };
  ws.onmessage = function(event) {
    var data = JSON.parse(event.data);
    drawChart(data);
  };
</script></body></html>)rawliteral";

// --- KONFIGURACJA I2S ---
i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLING_FREQ,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 8,
  .dma_buf_len = 128,
  .use_apll = false
};

i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_SD };

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password); 
  
  Serial.println("AP uruchomiony!");
  Serial.print("IP adresu: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  int16_t buffer[SAMPLES];
  size_t bytesRead;
  i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
  
  for (int i = 0; i < SAMPLES; i++) { vReal[i] = buffer[i]; vImag[i] = 0; }
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  // Throttling - wysylaj max co ~100ms, nie zalewaj WebSocketa
  if (ws.count() > 0 && millis() - lastSend > 100) {
    lastSend = millis();
    ws.cleanupClients();

    String json = "[";
    for(int i = 0; i < SAMPLES/2; i++) {
      json += String((int)vReal[i]); // int zamiast double - krotszy JSON
      if(i < SAMPLES/2 - 1) json += ",";
    }
    json += "]";
    ws.textAll(json);
  }
}