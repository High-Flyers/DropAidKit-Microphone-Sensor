#ifndef DROPPER_H
#define DROPPER

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Konfiguracja pinów (dostosuj do swojej płytki) ---- */
#define DROPPER_SERVO_GPIO     GPIO_NUM_1
#define DROPPER_LIMIT_GPIO     GPIO_NUM_10

/* ---- Pozycje serwa w mikrosekundach szerokości impulsu ---- */
#define SERVO_CLOSED            1000    // pozycja zamknięta - payload załadowany
#define SERVO_OPEN              2000    // pozycja otwarta - zrzut payloadu

#define TIMEOUT 5000

/**
 * @brief Inicjalizuje serwo (LEDC) oraz pin krańcówki, ustawia serwo
 *        w pozycji SERVO_CLOSED.
 *
 * @return ESP_OK w przypadku sukcesu, w przeciwnym razie kod błędu.
 */
esp_err_t dropper_init(void);

/**
 * @brief Sprawdza stan krańcówki, żeby ustalić czy payload jest załadowany.
 *
 * @return true jeśli payload jest wykryty (krańcówka wciśnięta),
 *         false jeśli brak payloadu.
 */
bool dropper_check(void);

/**
 * @brief Wykonuje zrzut: przesuwa serwo do pozycji SERVO_OPEN,
 *        odczekuje 1 sekundę, po czym sprawdza krańcówkę.
 *
 * @return true jeśli krańcówka się zwolniła (zrzut udany),
 *         false jeśli payload nadal jest wykrywany (zrzut nieudany).
 */
bool dropper_drop(void);

#ifdef __cplusplus
}
#endif


#endif // DROPPER_H
