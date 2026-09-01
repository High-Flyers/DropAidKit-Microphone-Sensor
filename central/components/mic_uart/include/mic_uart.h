#ifndef MIC_UART_H
#define MIC_UART_H

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define ERROR_DATA 0
#define NEW_DATA 1
#define WAIT_DATA 2

// Inicjalizacja drugiego UART-a 
void init_second_uart(void);
int read_mic_data(int *freq, int *amp);

#endif // MIC_UART_H