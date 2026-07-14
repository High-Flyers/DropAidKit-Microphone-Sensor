#include <driver/i2s.h>
#include <arduinoFFT.h>

// I2S pins
#define I2S_WS 7
#define I2S_SD 5
#define I2S_SCK 6

// FFT settings
#define SAMPLES 256
#define SAMPLING_FREQ 16000

double vReal[SAMPLES];
double vImag[SAMPLES];

// jawnie określamy typ szablonu double
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, (double)SAMPLING_FREQ);

// I2S configuration
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

void setup() {

  Serial.begin(115200);

  // start I2S
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  Serial.println("Start analizy audio...");
}

void loop() {

  int16_t buffer[SAMPLES];
  size_t bytesRead;

  // read samples
  i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);

  int samples = bytesRead / 2;

  for (int i = 0; i < samples; i++) {
    vReal[i] = buffer[i];
    vImag[i] = 0;
  }

  // FFT
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  double peak = FFT.majorPeak();

  // print całego widma
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