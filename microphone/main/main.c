#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "inmp441_mic.h"

bool meas_on = true;

void mic_task(void *arg) {
    acoustic_metrics_t metrics;
    while (1) {
        if (meas_on) 
        {	
			if (inmp441_get_metrics(&metrics))
			{
           		printf("meas: %d %d\n", (int)(metrics.peak_frequency_hz), (int)(metrics.peak_amplitude *1000));
           	}
           	else 
           		printf("meas: error\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{	
	printf("booting...\n");

	inmp441_init(6, 7, 5);
	
	xTaskCreate(mic_task, "mic_task", 8192, NULL, 5, NULL);
}
