#include "mic_uart.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define UART_PORT_NUM      UART_NUM_1
#define UART_BAUD_RATE     115200
#define UART_TX_PIN        5      // Pin TX (możesz zmienić)
#define UART_RX_PIN        6      // Pin RX (możesz zmienić)
#define UART_BUF_SIZE      1024

static const char *TAG = "MIC_UART";

void init_second_uart(void) {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Konfiguracja i instalacja sterownika UART1
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    
    // Uruchomienie taska FreeRTOS w tle
   	// xTaskCreate(uart1_read_task, "uart1_task", 2048, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "Moduł UART mikrofonu zainicjalizowany (TX:%d, RX:%d)", UART_TX_PIN, UART_RX_PIN);
}

int read_mic_data(int *freq, int *amp) {
    uint8_t data[128];
    
    // Odbiór danych z UART1 (timeout 100ms)
	int len = uart_read_bytes(UART_PORT_NUM, data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        
	if (len > 0) 
        {
        data[len] = '\0'; // Zamykamy stringa
            
        // Przetwarzamy dane tylko wtedy, gdy komenda "meas on" jest aktywna
                
        // Parsowanie formatu: "meas: <czestotliwosc> <amplituda>"
        // np. "meas: 440.5 12.3"
        if (sscanf((char *)data, "meas: %d %d", freq, amp) == 2) 
        {
           //ESP_LOGI(TAG, "Pomiar -> Freq: %.2f Hz | Amp: %.2f", *freq, *amp);   
           return NEW_DATA;                
        } 
        else 
        {
            // Jeśli przyszedł śmietnik albo inny format
            //ESP_LOGD(TAG, "Odebrano surowe dane (brak dopasowania ramki): %s", data);
            return ERROR_DATA;
        }
   	}
   	return WAIT_DATA;
}