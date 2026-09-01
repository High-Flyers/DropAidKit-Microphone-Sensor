#include "inmp441_mic.h"

#include "driver/i2s_std.h"
#include "esp_dsp.h"
#include "esp_log.h"

#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "INMP441";

#define SAMPLE_RATE     32000
#define FFT_N           1024

static i2s_chan_handle_t rx_handle = NULL;

static float fft_buffer[FFT_N * 2];
static float hann_window[FFT_N];

static bool fft_initialized = false;

bool inmp441_init(int sck_pin, int ws_pin, int sd_pin)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    esp_err_t ret = i2s_new_channel(
        &chan_cfg,
        NULL,
        &rx_handle
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return false;
    }
	
	i2s_std_slot_config_t slot_cfg =
	    I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
	slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    i2s_std_config_t std_cfg = {

        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),

        .slot_cfg = slot_cfg,
        
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = sck_pin,
            .ws   = ws_pin,
            .dout = I2S_GPIO_UNUSED,
            .din  = sd_pin,

            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    /*
     * INMP441 wysyła 24-bit audio w 32-bitowej ramce.
     *
     * Jeśli Twój INMP441 ma L/R do GND, powinien być odczytywany
     * jako LEFT. Jeżeli L/R jest do VDD, trzeba użyć RIGHT.
     */

    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "i2s_channel_init_std_mode failed: %s",
                 esp_err_to_name(ret));
        return false;
    }

    ret = i2s_channel_enable(rx_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "i2s_channel_enable failed: %s",
                 esp_err_to_name(ret));
        return false;
    }

    /*
     * ESP-DSP FFT
     */
    ret = dsps_fft2r_init_fc32(NULL, FFT_N);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "dsps_fft2r_init_fc32 failed: %s",
                 esp_err_to_name(ret));
        return false;
    }

    /*
     * Hann window
     */
    dsps_wind_hann_f32(hann_window, FFT_N);

    fft_initialized = true;

    ESP_LOGI(TAG,
             "INMP441 initialized: %d Hz, FFT=%d",
             SAMPLE_RATE,
             FFT_N);

    return true;
}


bool inmp441_get_metrics(acoustic_metrics_t *metrics)
{
    if (!rx_handle || !metrics || !fft_initialized) {
        return false;
    }

    static int32_t raw_buffer[FFT_N];

    size_t bytes_read = 0;

    esp_err_t ret = i2s_channel_read(
        rx_handle,
        raw_buffer,
        sizeof(raw_buffer),
        &bytes_read,
        pdMS_TO_TICKS(1000)
    );

    if (ret != ESP_OK || bytes_read != sizeof(raw_buffer)) {

        ESP_LOGW(TAG,
                 "I2S read failed: %s, bytes=%u",
                 esp_err_to_name(ret),
                 (unsigned)bytes_read);

        return false;
    }
    
    /* debug
    for (int i = 0; i < 20; i++) {
	    printf("%ld\n", (long)(raw_buffer[i] >> 8));
	}
	*/
		
    /*
     * ---------------------------------------------------------
     * 1. Konwersja INMP441 -> float
     * ---------------------------------------------------------
     *
     * INMP441:
     *
     * 24-bit signed PCM
     * zapakowany w 32-bitową ramkę I2S.
     *
     * W typowej konfiguracji MSB-first dane są wyrównane
     * do lewej strony 32-bitowego słowa.
     *
     * Dlatego przesuwamy o 8 bitów.
     */

    float mean = 0.0f;

    for (int i = 0; i < FFT_N; i++) {

        int32_t sample = raw_buffer[i] >> 8;

        /*
         * 24-bit signed:
         * zakres około [-8388608, 8388607]
         */

        float x = (float)sample / 8388608.0f;

        mean += x;
    }

    /*
     * ---------------------------------------------------------
     * 2. DC offset
     * ---------------------------------------------------------
     */

    mean /= FFT_N;


    /*
     * ---------------------------------------------------------
     * 3. Przygotowanie FFT
     * ---------------------------------------------------------
     */

    float coherent_gain = 0.0f;

    for (int i = 0; i < FFT_N; i++) {

        int32_t sample = raw_buffer[i] >> 8;

        float x = (float)sample / 8388608.0f;

        /*
         * usuń DC
         */
        x -= mean;

        /*
         * Hann window
         */
        x *= hann_window[i];

        /*
         * ESP-DSP complex buffer
         */
        fft_buffer[2 * i]     = x;
        fft_buffer[2 * i + 1] = 0.0f;

        coherent_gain += hann_window[i];
    }


    /*
     * ---------------------------------------------------------
     * 4. FFT
     * ---------------------------------------------------------
     */

    dsps_fft2r_fc32(fft_buffer, FFT_N);

    dsps_bit_rev_fc32(fft_buffer, FFT_N);
    
    /* debug
	printf("FFT:\n");
	
	for (int bin = 0; bin < 20; bin++) {
	    float real = fft_buffer[2 * bin];
	    float imag = fft_buffer[2 * bin + 1];
	
	    float magnitude = sqrtf(real * real + imag * imag);
	
	    printf("%d: %f\n", bin, magnitude);
	}
	*/
	
     /*
     * ---------------------------------------------------------
     * 5. Szukanie maksimum TYLKO 2-4 kHz
     * ---------------------------------------------------------
     */

    const float freq_resolution =
        (float)SAMPLE_RATE / (float)FFT_N;

    const float SEARCH_MIN_HZ = 2000.0f;
    const float SEARCH_MAX_HZ = 4000.0f;

    int bin_min = (int)ceilf(SEARCH_MIN_HZ / freq_resolution);
    int bin_max = (int)floorf(SEARCH_MAX_HZ / freq_resolution);

    if (bin_min < 1)
        bin_min = 1;

    if (bin_max >= FFT_N / 2)
        bin_max = FFT_N / 2 - 1;


    /*
     * ---------------------------------------------------------
     * 6. Znajdź największą amplitudę w zakresie 2-4 kHz
     * ---------------------------------------------------------
     */

    float max_magnitude = 0.0f;
    int peak_bin = bin_min;

    for (int bin = bin_min; bin <= bin_max; bin++) {

        float real = fft_buffer[2 * bin];
        float imag = fft_buffer[2 * bin + 1];

        float magnitude =
            sqrtf(
                real * real +
                imag * imag
            );

        if (magnitude > max_magnitude) {

            max_magnitude = magnitude;
            peak_bin = bin;
        }
    }


    /*
     * ---------------------------------------------------------
     * 7. Interpolacja paraboliczna peaku
     * ---------------------------------------------------------
     *
     * Dzięki temu nie jesteśmy ograniczeni do 15.625 Hz/bin.
     */

    float peak_offset = 0.0f;

    if (peak_bin > bin_min && peak_bin < bin_max) {

        float mag_left =
            sqrtf(
                fft_buffer[2 * (peak_bin - 1)] *
                fft_buffer[2 * (peak_bin - 1)] +

                fft_buffer[2 * (peak_bin - 1) + 1] *
                fft_buffer[2 * (peak_bin - 1) + 1]
            );

        float mag_center =
            sqrtf(
                fft_buffer[2 * peak_bin] *
                fft_buffer[2 * peak_bin] +

                fft_buffer[2 * peak_bin + 1] *
                fft_buffer[2 * peak_bin + 1]
            );

        float mag_right =
            sqrtf(
                fft_buffer[2 * (peak_bin + 1)] *
                fft_buffer[2 * (peak_bin + 1)] +

                fft_buffer[2 * (peak_bin + 1) + 1] *
                fft_buffer[2 * (peak_bin + 1) + 1]
            );

        float denominator =
            mag_left -
            2.0f * mag_center +
            mag_right;

        if (fabsf(denominator) > 1e-12f) {

            peak_offset =
                0.5f *
                (mag_left - mag_right) /
                denominator;

            /*
             * Bezpiecznik.
             */
            if (peak_offset > 0.5f)
                peak_offset = 0.5f;

            if (peak_offset < -0.5f)
                peak_offset = -0.5f;
        }
    }


    /*
     * ---------------------------------------------------------
     * 8. Peak frequency
     * ---------------------------------------------------------
     */

    float peak_frequency =
        ((float)peak_bin + peak_offset) *
        freq_resolution;


    /*
     * ---------------------------------------------------------
     * 9. Amplituda
     * ---------------------------------------------------------
     *
     * Jednostronne widmo:
     *
     * amplitude =
     *     2 * magnitude / coherent_gain
     */

    float amplitude = 0.0f;

    if (coherent_gain > 0.0f) {

        amplitude =max_magnitude;
            //(2.0f * max_magnitude) /
            //coherent_gain;
    }


    /*
     * ---------------------------------------------------------
     * 10. Wynik
     * ---------------------------------------------------------
     */

    metrics->peak_frequency_hz = peak_frequency;
    metrics->peak_amplitude    = amplitude;

    return true;
}