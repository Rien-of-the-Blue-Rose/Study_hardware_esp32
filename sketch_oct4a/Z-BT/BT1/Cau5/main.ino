// src/main.cpp
#include <Arduino.h>

const uint8_t LED_TARGET_PIN = 2;
const uint8_t BTN_UP_PIN     = 0;
const uint8_t BTN_DOWN_PIN   = 25;

const uint32_t PWM_FREQ = 1000;
const uint8_t  PWM_RES  = 8;
const uint8_t  MAX_LEVEL = 10;
uint8_t brightnessLevel = 0;

const uint8_t GAMMA_LEVELS[MAX_LEVEL + 1] = {0, 3, 8, 16, 28, 45, 72, 110, 155, 205, 255};

struct Button {
    uint8_t pin;
    uint8_t lastRaw = HIGH;
    uint8_t stable  = HIGH;
    unsigned long lastTime = 0;

    bool pressed() {
        uint8_t r = digitalRead(pin);
        unsigned long now = millis();
        if (r != lastRaw) {
            lastTime = now;
            lastRaw = r;
        }
        if ((now - lastTime >= 50) && (r != stable)) {
            stable = r;
            return stable == LOW;
        }
        return false;
    }
};

Button btnUp   = {BTN_UP_PIN};
Button btnDown = {BTN_DOWN_PIN};


void applyBrightness(int8_t dir = 0) {
    int8_t next = constrain(brightnessLevel + dir, 0, MAX_LEVEL);
    if (dir != 0 && next == brightnessLevel) {
        Serial.printf("[CANH BAO] Do sang da o muc %s (%u/10)!\n", 
                      dir > 0 ? "TOI DA" : "TAT", brightnessLevel);
        return;
    }
    brightnessLevel = next;
    uint8_t duty = GAMMA_LEVELS[brightnessLevel];
    ledcWrite(LED_TARGET_PIN, duty);

    Serial.printf("[SU KIEN: %s] -> Nac: %u/%u | Duty PWM: %3u/255 | Cong suat: %3u%%\n",
                  dir == 0 ? "KHOI TAO" : (dir > 0 ? "PHIM BOOT (TANG)" : "PHIM PHU (GIAM)"),
                  brightnessLevel, MAX_LEVEL, duty, (brightnessLevel * 100) / MAX_LEVEL);
}


void setup() {
    Serial.begin(115200);
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

    ledcAttach(LED_TARGET_PIN, PWM_FREQ, PWM_RES);
    applyBrightness(0);
}

void loop() {
    if (btnUp.pressed())   applyBrightness(1);
    if (btnDown.pressed()) applyBrightness(-1);
}
