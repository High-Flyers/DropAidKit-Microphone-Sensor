#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "console_uart.h"
#include "mic_uart.h"
#include "dropper.h"

void mic_task(void *arg) {
    int freq = 0; int amp = 0;
    
    while (1) {
        if (meas_on) 
        {	
			int ret = read_mic_data(&freq, &amp);
			if (ret == NEW_DATA)
			{
           		printf("meas: %d %d\n", freq, amp);
           	}
           	else if (ret == ERROR_DATA)
           	{
           		printf("meas: error\n");
           	}
           	else if (ret == WAIT_DATA)
           	{}

        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{	
	printf("booting...\n");
	shell_init();
	dropper_init();
	init_second_uart();
	
	xTaskCreate(mic_task, "mic_task", 8192, NULL, 5, NULL);
}
