# ESP32-C3 Dropper & Audio Unit (INMP441 + Servo)

Urządzenie pomiarowe oparte na mikrokontrolerze **ESP32-C3**, wyposażone w cyfrowy mikrofon **INMP441**, serwomechanizm obsługujący mechanizm zrzutu (dropper) oraz krańcówkę (switch) monitorującą pozycję.

## Architektura i Sprzęt (Hardware Pinout)

### 1. Mikrofon INMP441 (I2S)
| INMP441 Pin | ESP32-C3 Pin | Opis |
| :--- | :--- | :--- |
| **SCK** (BCLK) | GPIO 6 |
| **WS** (LRC) | GPIO 7 | 
| **SD** (DATA) | GPIO 5 | 

### 2. Servo & Krańcówka (Dropper)
| Komponent | ESP32-C3 Pin | Opis |
| :--- | :--- | :--- |
| **Serwomechanizm** | GPIO 1 | Sterowanie PWM (zrzut) |
| **Krańcówka** | GPIO 10 | Wejście cyfrowe (z podciągnięciem INPUT_PULLUP) |

### 3. Komunikacja UART (Shell)
| UART Pin | ESP32-C3 Pin | Opis |
| :--- | :--- | :--- |
| **TX** | GPIO 21 (domyślny TX) | Transmisja danych z ESP |
| **RX** | GPIO 20 (domyślny RX) | Odbiór danych do ESP |
| *Baudrate* | **115200** | Prędkość transmisji |

---

## Interfejs UART (Shell)

Urządzenie komunikuje się poprzez konsolę szeregową (UART). Po uruchomieniu i nawiązaniu połączenia można wysyłać komendy tekstowe zakończone znakiem nowej linii (`\n` lub `\r\n`).

### Dostępne komendy:

*   `drop` – Wyzwala mechanizm zrzutu (uruchamia serwo).
Odpowiedzi:
*   `no payload` - Brak ładunku w komorze zrzutu.
*	`dropping...` - Trwa zrzut.
*   `ready` – Zrzut powiódł się i krańcówka zmieniła stan (sygnał powrotny odebrany).
*   `error` – Upłynął limit czasu (**5 sekund**) i krańcówka nie zgłosiła gotowości (możliwe zacięcie mechanizmu).

*   `meas on/off` – Wyzwala mechanizm zrzutu (uruchamia serwo).
Odpowiedzi:
*   `meas turned on/off` - Potwierdzenie włączenia/wyłączenia pomiarów mikrofonem.
*	`meas: <częstotliwość w Hz> <amplituda 0-1000>` - Przykładowa ramka pomiarów `meas: 2700 234`.
*	`meas: error` - Błąd odczytu z mikrofonu.

*   `reset` – Reset mikrokontrolera.
Odpowiedzi:
*   `restarting...` - Resetowanie systemu.
*   `booting...` - Uruchamianie systemu.

*   `check` – Sprawdzenie czy jest ładunek.
Odpowiedzi:
*   `payload detected` - Ładunek obecny.
*   `no payload` - Brak ładunku.

---