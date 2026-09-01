#include "dropper.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "dropper";

/* ---- Konfiguracja LEDC ----
 * C3 ma tylko LOW_SPEED_MODE (brak trybu high-speed).
 * Rozdzielczość 14-bit */
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_CHANNEL       LEDC_CHANNEL_0
#define LEDC_DUTY_RES      LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY_HZ  50U            // 20ms okres - standard dla serw hobby
#define SERVO_PERIOD_US    20000U         // 1000000 / 50Hz

/* Załóżmy krańcówkę jako active-low z wewnętrznym pull-up:
 * 0 = wciśnięta (payload załadowany), 1 = zwolniona (brak payloadu). */
#define LIMIT_PAYLOAD_PRESENT  0

static uint32_t pulse_us_to_duty(uint32_t pulse_us)
{
    uint32_t max_duty = (1U << LEDC_DUTY_RES) - 1U;
    return (uint32_t)(((uint64_t)pulse_us * max_duty) / SERVO_PERIOD_US);
}

static esp_err_t servo_set_pulse_us(uint32_t pulse_us)
{
    esp_err_t err = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, pulse_us_to_duty(pulse_us));
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

esp_err_t dropper_init(void)
{
    esp_err_t err;

    /* --- pin krańcówki --- */
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << DROPPER_LIMIT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config (limit) nieudane: %s", esp_err_to_name(err));
        return err;
    }

    /* --- timer LEDC --- */
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = LEDC_FREQUENCY_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config nieudane: %s", esp_err_to_name(err));
        return err;
    }

    /* --- kanał LEDC (wyjście PWM na pinie serwa) --- */
    ledc_channel_config_t channel_config = {
        .gpio_num   = DROPPER_SERVO_GPIO,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config nieudane: %s", esp_err_to_name(err));
        return err;
    }

    /* pozycja startowa - zamknięte (payload zabezpieczony) */
    err = servo_set_pulse_us(SERVO_CLOSED);
    if (err != ESP_OK) {
        return err;
    }

    //ESP_LOGI(TAG, "dropper zainicjalizowany, pozycja: SERVO_CLOSED (%d us)", SERVO_CLOSED);
    return ESP_OK;
}

bool dropper_check(void)
{
    int level = gpio_get_level(DROPPER_LIMIT_GPIO);
    bool payload_present = (level == LIMIT_PAYLOAD_PRESENT);

   // ESP_LOGI(TAG, "krancowka: %s", payload_present ? "payload zaladowany" : "brak payloadu");
    return payload_present;
}

bool dropper_drop(void)
{
    //ESP_LOGI(TAG, "start zrzutu - otwieranie serwa (%d us)", SERVO_OPEN);
    servo_set_pulse_us(SERVO_OPEN);
	int level = 0;
	
	uint64_t t_start = esp_timer_get_time();
	uint64_t now = t_start;
	
	do 
	{
    	level = gpio_get_level(DROPPER_LIMIT_GPIO);
    	vTaskDelay(pdMS_TO_TICKS(100));
    	
    	now = esp_timer_get_time();
    }
    while (level == 0 && (now - t_start)/ 1000 <= TIMEOUT);
    
    bool released = (level != LIMIT_PAYLOAD_PRESENT);

    //ESP_LOGI(TAG, "wynik zrzutu: %s",
             //released ? "OK - krancowka zwolniona" : "BLAD - payload nadal wykryty");

    return released;
}
