#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// --- KONFIGURACJA WIFI ---
IPAddress local_IP(192, 168, 0, 200);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

const char* ssid = "ESP32_MIC_AP";
const char* password = "hf21372137";

// --- PINY ---
#define BUZZER_PIN 2

// I2S (INMP441)
#define I2S_WS 7
#define I2S_SD 5
#define I2S_SCK 6

// Analog (MAX4466)
#define MIC_ADC_PIN 0   // UWAGA: GPIO0 to pin strapping (boot mode) na ESP32 -
                        // jeśli masz problemy z wgrywaniem/bootem, podłącz mikrofon do innego pinu ADC1 (np. GPIO1, GPIO4)

// --- FFT ---
#define SAMPLES 256
#define SAMPLING_FREQ 16000

double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, (double)SAMPLING_FREQ);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- WYBÓR MIKROFONU (sterowany ze strony webowej) ---
// 0 = MAX4466 (analogowy), 1 = INMP441 (I2S)
volatile int micMode = 1; // domyślnie INMP441 (polecany do drona - patrz wcześniejsza rozmowa)

// --- HTML / JS INTERFEJS ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<meta charset="UTF-8">
<style>
  body { font-family: Arial; text-align: center; background:#111; color:#eee; margin:0; padding:10px; }
  canvas { background:#000; border:1px solid #444; width:100%; max-width:800px; height:400px; }
  .mic-select { margin: 15px auto; max-width:800px; text-align:left; background:#1a1a1a; padding:12px 16px; border-radius:8px; }
  .mic-select label { margin-right: 25px; font-size: 16px; cursor: pointer; }
  .mic-select input { margin-right: 6px; cursor: pointer; }
  #status { color:#4da3ff; font-size:14px; margin-top:6px; }
  h2 { margin-bottom: 5px; }
  .stats { margin: 15px auto; max-width:800px; display:flex; gap:10px; justify-content:space-between; }
  .stat-box { flex:1; background:#1a1a1a; border-radius:8px; padding:12px; text-align:center; }
  .stat-box .label { font-size:12px; color:#888; text-transform:uppercase; letter-spacing:0.5px; }
  .stat-box .value { font-size:24px; color:#4da3ff; font-weight:bold; margin-top:4px; }
  .stat-box.snr-ok .value { color:#4dff88; }
  .stat-box.snr-bad .value { color:#ff4d4d; }
</style>
</head><body>
<h2>Rozklad FFT</h2>

<div class="mic-select">
  <strong>Zrodlo mikrofonu:</strong><br><br>
  <label><input type="radio" name="mic" value="inmp" id="micInmp"> INMP441 (I2S, cyfrowy)</label>
  <label><input type="radio" name="mic" value="max4466" id="micMax"> MAX4466 (analogowy)</label>
  <div id="status">Aktualne zrodlo: ---</div>
</div>

<div class="stats">
  <div class="stat-box">
    <div class="label">Peak Frequency</div>
    <div class="value" id="peakFreq">--- Hz</div>
  </div>
  <div class="stat-box">
    <div class="label">Peak Magnitude</div>
    <div class="value" id="peakMag">---</div>
  </div>
  <div class="stat-box" id="snrBox">
    <div class="label">SNR</div>
    <div class="value" id="snr">--- dB</div>
  </div>
</div>

<canvas id="fftChart" width="800" height="400"></canvas>
<script>
  var canvas = document.getElementById('fftChart');
  var ctx = canvas.getContext('2d');
  var W = canvas.width, H = canvas.height;
  var barCount = 128;
  var freqStep = 62.5; // SAMPLING_FREQ / SAMPLES = 16000/256

  var marginLeft = 45;
  var marginBottom = 20;
  var plotW = W - marginLeft;
  var plotH = H - marginBottom;

  function drawChart(data) {
    ctx.clearRect(0, 0, W, H);

    var maxVal = Math.max(...data, 1000);
    var barWidth = plotW / barCount;

    var ySteps = 5;
    ctx.strokeStyle = '#333';
    ctx.fillStyle = '#888';
    ctx.font = '10px Arial';
    ctx.textAlign = 'right';

    for (var s = 0; s <= ySteps; s++) {
      var val = Math.round((maxVal / ySteps) * s);
      var y = plotH - (plotH * s / ySteps);

      ctx.beginPath();
      ctx.moveTo(marginLeft, y);
      ctx.lineTo(W, y);
      ctx.stroke();

      ctx.fillText(val, marginLeft - 5, y + 3);
    }

    for (var i = 5; i < data.length; i++) {
      var val = data[i];
      var barHeight = (val / maxVal) * plotH;
      var x = marginLeft + i * barWidth;
      var y = plotH - barHeight;

      ctx.fillStyle = '#4da3ff';
      ctx.fillRect(x, y, barWidth - 1, barHeight);
    }

    ctx.fillStyle = '#888';
    ctx.textAlign = 'left';
    for (var i = 5; i < data.length; i += 16) {
      var freq = Math.round(i * freqStep);
      ctx.fillText(freq + 'Hz', marginLeft + i * barWidth, H - 5);
    }

    ctx.save();
    ctx.translate(12, plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = 'center';
    ctx.fillText('Amplituda', 0, 0);
    ctx.restore();
  }

  var ws = new WebSocket(`ws://${window.location.hostname}/ws`);

  ws.onopen = function() {
    console.log('WS polaczony');
  };
  ws.onerror = function(e) { console.log('WS blad', e); };

  ws.onmessage = function(event) {
    // serwer wysyla: tablice FFT (zaczyna sie od '['), obiekt ze statystykami (zaczyna sie od '{'),
    // albo status trybu (zaczyna sie od 'MODE:')
    var msg = event.data;
    if (msg.charAt(0) === '[') {
      var data = JSON.parse(msg);
      drawChart(data);
    } else if (msg.charAt(0) === '{') {
      var stats = JSON.parse(msg);
      updateStats(stats);
    } else if (msg.indexOf('MODE:') === 0) {
      var mode = msg.substring(5);
      updateStatusDisplay(mode);
    }
  };

  function updateStats(s) {
    document.getElementById('peakFreq').innerText = s.peakFreq.toFixed(1) + ' Hz';
    document.getElementById('peakMag').innerText = s.peakMag.toFixed(0);

    var snrEl = document.getElementById('snr');
    var snrBox = document.getElementById('snrBox');
    snrEl.innerText = s.snrDb.toFixed(1) + ' dB';

    // prosty wizualny wskaznik: SNR > 10dB = "dobry" sygnal
    snrBox.classList.remove('snr-ok', 'snr-bad');
    snrBox.classList.add(s.snrDb > 10 ? 'snr-ok' : 'snr-bad');
  }

  function updateStatusDisplay(mode) {
    if (mode === 'inmp') {
      document.getElementById('micInmp').checked = true;
      document.getElementById('status').innerText = 'Aktualne zrodlo: INMP441 (I2S)';
    } else {
      document.getElementById('micMax').checked = true;
      document.getElementById('status').innerText = 'Aktualne zrodlo: MAX4466 (ADC)';
    }
  }

  document.querySelectorAll('input[name="mic"]').forEach(function(radio) {
    radio.addEventListener('change', function() {
      if (this.checked && ws.readyState === WebSocket.OPEN) {
        ws.send('MODE:' + this.value);
      }
    });
  });
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

i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_SCK,
  .ws_io_num = I2S_WS,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num = I2S_SD
};

const uint32_t sampleIntervalUs = 1000000UL / SAMPLING_FREQ;
unsigned long lastSend = 0;

// --- ODCZYT Z MAX4466 (ADC, stały sample rate) ---
void readFromMax4466() {
  uint32_t nextSampleTime = micros();
  for (int i = 0; i < SAMPLES; i++) {
    while ((int32_t)(micros() - nextSampleTime) < 0) {
      // czekamy na kolejny slot czasowy, zeby zachowac staly sample rate
    }
    int raw = analogRead(MIC_ADC_PIN);   // 0-4095
    vReal[i] = (double)raw - 2048.0;     // centrowanie wokol zera
    vImag[i] = 0;
    nextSampleTime += sampleIntervalUs;
  }
}

// --- ODCZYT Z INMP441 (I2S/DMA) ---
void readFromINMP441() {
  int16_t buffer[SAMPLES];
  size_t bytesRead;

  i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
  int samples = bytesRead / 2;

  for (int i = 0; i < samples; i++) {
    vReal[i] = buffer[i];
    vImag[i] = 0;
  }
  for (int i = samples; i < SAMPLES; i++) {
    vReal[i] = 0;
    vImag[i] = 0;
  }
}

// --- OBSLUGA WIADOMOSCI Z PRZEGLADARKI (przelaczanie mikrofonu) ---
void handleWsMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    String msg = String((char*)data, len);

    if (msg == "MODE:inmp") {
      micMode = 1;
      Serial.println("Przelaczono na INMP441 (z przegladarki)");
      ws.textAll("MODE:inmp");
    } else if (msg == "MODE:max4466") {
      micMode = 0;
      Serial.println("Przelaczono na MAX4466 (z przegladarki)");
      ws.textAll("MODE:max4466");
    }
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Klient WS #%u polaczony\n", client->id());
    // wyslij aktualny stan trybu do nowo polaczonego klienta
    client->text(micMode == 1 ? "MODE:inmp" : "MODE:max4466");
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Klient WS #%u rozlaczony\n", client->id());
  } else if (type == WS_EVT_DATA) {
    handleWsMessage(arg, data, len);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  analogReadResolution(12); // 0-4095

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("AP uruchomiony!");
  Serial.print("IP adresu: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  if (micMode == 1) {
    readFromINMP441();
  } else {
    readFromMax4466();
  }

  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  double peakFreq = FFT.majorPeak(); // czestotliwosc peaku (z interpolacja, dokladniejsza niz sam numer binu)

  // znajdz bin peaku i jego magnitude (pomijamy bin 0 = DC)
  int peakBin = 1;
  double peakMag = vReal[1];
  for (int i = 2; i < SAMPLES / 2; i++) {
    if (vReal[i] > peakMag) {
      peakMag = vReal[i];
      peakBin = i;
    }
  }

  // poziom szumu tla = srednia ze wszystkich binow POZA okolica peaku (guard band)
  const int guard = 3; // ile binow z kazdej strony peaku wykluczamy z liczenia szumu
  double noiseSum = 0;
  int noiseCount = 0;
  for (int i = 1; i < SAMPLES / 2; i++) {
    if (abs(i - peakBin) > guard) {
      noiseSum += vReal[i];
      noiseCount++;
    }
  }
  double noiseFloor = (noiseCount > 0) ? (noiseSum / noiseCount) : 1.0;
  if (noiseFloor < 1.0) noiseFloor = 1.0; // zabezpieczenie przed dzieleniem przez ~0

  double snrDb = 20.0 * log10(peakMag / noiseFloor); // 20*log10 bo to stosunek amplitud, nie mocy

  // Throttling - wysylaj max co ~100ms, nie zalewaj WebSocketa
  if (ws.count() > 0 && millis() - lastSend > 100) {
    lastSend = millis();
    ws.cleanupClients();

    String json = "[";
    for (int i = 0; i < SAMPLES / 2; i++) {
      json += String((int)vReal[i]);
      if (i < SAMPLES / 2 - 1) json += ",";
    }
    json += "]";
    ws.textAll(json);

    String statsJson = "{\"peakFreq\":" + String(peakFreq, 1) +
                        ",\"peakMag\":" + String(peakMag, 1) +
                        ",\"snrDb\":" + String(snrDb, 1) + "}";
    ws.textAll(statsJson);
  }
}
