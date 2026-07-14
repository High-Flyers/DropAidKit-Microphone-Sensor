#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

IPAddress local_IP(192, 168, 0, 201); 
IPAddress gateway(192, 168, 0, 1);   
IPAddress subnet(255, 255, 255, 0);

// --- KONFIGURACJA ---
const char* ssid = "xxx";
const char* password = "xxx";
#define BUZZER_PIN 2 // Ustaw swój pin buzzera
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

// --- HTML / JS INTERFEJS ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.0/chart.min.js"></script>
<style>body { font-family: Arial; text-align: center; }</style>
</head><body>
<h2>Rozklad FFT</h2>
<canvas id="fftChart"></canvas><br>
<script>
  var ctx = document.getElementById('fftChart').getContext('2d');
  var labels = Array.from({length: 128}, (_, i) => Math.round(i * 62.5)); // Tu obliczamy Hz
  var chart = new Chart(ctx, { 
      type: 'line', 
      data: { 
          labels: labels, // Używamy obliczonych etykiet
          datasets: [{ data: [], label: 'FFT (Hz)', borderColor: 'blue' }] 
      }, 
      options: { 
          animation: false,
          scales: {
              x: { title: { display: true, text: 'Czestotliwosc [Hz]' } }
          }
      }
  });
  var ws = new WebSocket(`ws://${window.location.hostname}/ws`);
  ws.onmessage = function(event) { chart.data.datasets[0].data = JSON.parse(event.data); chart.update('none'); };
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

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println(WiFi.localIP());

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

  if(ws.count() > 0) {
    String json = "[";
    for(int i = 0; i < SAMPLES/2; i++) {
      json += String(vReal[i]);
      if(i < SAMPLES/2 - 1) json += ",";
    }
    json += "]";
    ws.textAll(json);
  }
}