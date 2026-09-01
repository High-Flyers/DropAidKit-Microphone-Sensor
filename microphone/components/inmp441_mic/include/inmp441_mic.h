#ifndef INMP441_MIC_H
#define INMP441_MIC_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Struktura przechowująca wyniki analizy akustycznej pasma 2-4 kHz
 */
typedef struct {
    float peak_frequency_hz; // Częstotliwość dominująca w paśmie 2-4 kHz (w Hz)
    float peak_amplitude;    // Amplituda/moc tego piku
} acoustic_metrics_t;

/**
 * @brief Inicjalizacja mikrofonu INMP441 po I2S
 * @param sck_pin Pin BCLK (Serial Clock)
 * @param ws_pin  Pin LRC (Word Select)
 * @param sd_pin  Pin DATA (Serial Data)
 * @return true jeśli sukces, false w przeciwnym razie
 */
bool inmp441_init(int sck_pin, int ws_pin, int sd_pin);

/**
 * @brief Pobiera próbki audio i oblicza metryki (peak freq i amplituda w 2-4 kHz)
 * @   Wskaźnik na strukturę wynikową
 * @return true jeśli pomiar się powiódł
 */
bool inmp441_get_metrics(acoustic_metrics_t *metrics);

void inmp441_debug_zero_crossing(void);

#endif // INMP441_MIC_H