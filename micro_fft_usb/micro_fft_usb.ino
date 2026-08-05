#include <driver/i2s.h>
#include <arduinoFFT.h>

// I2S pins (INMP441)
#define I2S_WS 7
#define I2S_SD 5
#define I2S_SCK 6

// Analog mic (MAX4466)
#define MIC_ADC_PIN 0      // <- podmień na pin, do którego podpięty jest OUT z MAX4466

// Pin przełączający
#define MODE_SWITCH_PIN 10 // 0 = MAX4466, 1 = INMP441 (I2S)

// FFT settings
#define SAMPLES 256
#define SAMPLING_FREQ 16000

double vReal[SAMPLES];
double vImag[SAMPLES];

ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, (double)SAMPLING_FREQ);

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

// interwał próbkowania dla trybu analogowego (w mikrosekundach)
const uint32_t sampleIntervalUs = 1000000UL / SAMPLING_FREQ;

void setup() {
  Serial.begin(115200);

  pinMode(MODE_SWITCH_PIN, INPUT_PULLDOWN); // domyślnie 0 -> MAX4466, jeśli nic nie podpięte

  analogReadResolution(12); // 0-4095

  // I2S startujemy zawsze, ale czytamy z niego tylko gdy tryb = INMP
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  Serial.println("Start analizy audio...");
}

void readFromMax4466() {
  uint32_t nextSampleTime = micros();
  for (int i = 0; i < SAMPLES; i++) {
    while ((int32_t)(micros() - nextSampleTime) < 0) {
      // czekamy na kolejny slot czasowy, żeby zachować stały sample rate
    }
    int raw = analogRead(MIC_ADC_PIN);   // 0-4095
    vReal[i] = (double)raw - 2048.0;     // centrowanie wokół zera (usuwamy DC offset)
    vImag[i] = 0;
    nextSampleTime += sampleIntervalUs;
  }
}

void readFromINMP441() {
  int16_t buffer[SAMPLES];
  size_t bytesRead;

  i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
  int samples = bytesRead / 2;

  for (int i = 0; i < samples; i++) {
    vReal[i] = buffer[i];
    vImag[i] = 0;
  }
  // gdyby bytesRead było mniejsze niż SAMPLES*2, dopełniamy zerami
  for (int i = samples; i < SAMPLES; i++) {
    vReal[i] = 0;
    vImag[i] = 0;
  }
}

void loop() {
  bool useI2S = digitalRead(MODE_SWITCH_PIN); // 1 = INMP441, 0 = MAX4466

  if (useI2S) {
    readFromINMP441();
  } else {
    readFromMax4466();
  }

  // FFT
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  double peak = FFT.majorPeak();

  Serial.print("Zrodlo: ");
  Serial.println(useI2S ? "INMP441 (I2S)" : "MAX4466 (ADC)");

  Serial.print("FFT:");
  for (int i = 0; i < (SAMPLES / 2); i++) {
    Serial.print(vReal[i]);
    if (i < (SAMPLES / 2 - 1)) Serial.print(",");
  }
  Serial.println();

  Serial.print("Czestotliwosc: ");
  Serial.print(peak);
  Serial.println(" Hz");

  delay(1000);
}