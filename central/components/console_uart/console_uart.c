#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "console_uart.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_console.h"

#include "dropper.h"

static const char *TAG = "SHELL";

bool meas_on;
// Handler dla komendy "drop"
static int cmd_drop(int argc, char **argv) {

    // 1. Sprawdzenie czy payload w ogole jest zaladowany
    if (!dropper_check()) {
        printf("no payload\n");
        return 1;
    }
	
	// 2. Info ze leci drop
	printf("dropping...\n");
	
    // 3. Uruchomienie serwa + oczekiwanie/sprawdzenie krancowki
    bool success = dropper_drop();

    // 4. Wynik
    if (success) {
        printf("ready\n");
        return 0; // Sukces dla konsoli
    } else {
        printf("error\n");
        return 1; // Blad dla konsoli
    }
}

// Handler info z mikrofonu
static int cmd_mic_meas(int argc, char **argv) {
	
	if (argc < 2) {
        printf("usage: meas <on|off>\n");
        return 0;
    }
    
    if      		(strcmp(argv[1], "on") == 0)
    {
		meas_on = true;
		printf("meas turned on\n");
	}
    else if      	(strcmp(argv[1], "off") == 0)
    {
		meas_on = false;
		printf("meas turned off\n");
	}
    return 0;
}

// Reset
static int cmd_reset(int argc, char **argv)
{
    printf("restarting...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return 0;
}

// Sprawdz czy jest ladunek
static int cmd_check_payload(int argc, char **argv)
{
    if (dropper_check())
    	printf("payload detected\n");
   	else
 		printf("no payload...\n");
    return 0;
}

// Rejestracja komendy w esp_console
void register_drop_cmd(void) {
	// drop
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "drop",
        .help = "Wyzwala serwo zrzutu i oczekuje na sygnal z krancowki",
        .hint = NULL,
        .func = cmd_drop,
	}));
	
	// microphone
	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "meas",
        .help    = "Wlacz lub wylacz pomiary mikrofonu",
        .hint    = "<on|off>",
        .func    = cmd_mic_meas,
    }));
	
	// reset
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "reset",
	    .help    = "Zresetuj urzadzenie",
	    .func    = cmd_reset,
	}));
	
	// sprawdz payload
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "check",
	    .help    = "Sprawdz czy jest payload",
	    .func    = cmd_check_payload,
	}));
	
}

void shell_init(void) {
    // Konfiguracja REPL (Read-Eval-Print Loop) dla UART
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "esp32c3> ";
    repl_config.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    
    esp_console_repl_t *repl = NULL;
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    // Rejestrujemy naszą komendę
   	register_drop_cmd();

    // Uruchomienie środowiska REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "Console shell zainicjalizowany pomyślnie.");
}