// path: src/main.cpp
#include <Arduino.h>

const uint8_t LED_PINS[] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = sizeof(LED_PINS);

const uint32_t PWM_FREQ  = 5000;
const uint8_t  PWM_RES   = 8;
const uint8_t  DUTY_BOOT = 0;   // Mức khởi động ban đầu (Tắt hoàn toàn)
const uint8_t  DUTY_MIN  = 7;   // Mức sàn duy trì sau chu kỳ 1 (Giữ chân sáng mờ)
const uint8_t  DUTY_MAX  = 255;
const uint8_t  FADE_STEP = 8;
const unsigned long INTERVAL = 45;
const unsigned long PAUSE_MS = 300;

inline void setLedDuty(uint8_t index, uint8_t duty) {
    ledcWrite(LED_PINS[index], duty);
}


uint8_t currentLed = 0;
uint8_t dutyMin = DUTY_BOOT;        // Bắt đầu chu trình đầu tiên tại mức 0
int16_t currentDuty = DUTY_BOOT;
int8_t fadeDir = 1;
unsigned long lastUpdate = 0;
unsigned long interval = INTERVAL;


void setup() {
    for (uint8_t pin : LED_PINS) {
        ledcAttach(pin, PWM_FREQ, PWM_RES);
        ledcWrite(pin, DUTY_BOOT);
    }
}

void loop() {
    unsigned long now = millis();
    if (now - lastUpdate < interval) return;
    lastUpdate = now;

    currentDuty += fadeDir * FADE_STEP;
    bool reached = (fadeDir > 0) ? (currentDuty >= DUTY_MAX) : (currentDuty <= dutyMin);
    if (reached) {
        currentDuty = (fadeDir > 0) ? DUTY_MAX : dutyMin;
    }

    setLedDuty(currentLed, currentDuty);

    if (reached) {
        if (++currentLed >= TOTAL_LEDS) {
            currentLed = 0;
            fadeDir = -fadeDir;
            dutyMin = DUTY_MIN;  // Chốt sàn độ sáng thành 7 sau khi kết thúc lượt quét đầu
            interval = PAUSE_MS; // Áp dụng khoảng nghỉ 300ms trước khi đảo chiều
        } else {
            interval = INTERVAL;
        }
        currentDuty = (fadeDir > 0) ? dutyMin : DUTY_MAX;
    } else {
        interval = INTERVAL;
    }
}
