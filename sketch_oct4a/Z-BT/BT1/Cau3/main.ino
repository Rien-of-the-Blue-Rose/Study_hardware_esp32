// src/main.cpp
#include <Arduino.h>

const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

// Cổng xuất trạng thái chuẩn hóa: 1 byte ánh xạ trực tiếp ra 8 chân LED
void setLedByte(uint8_t state) {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        digitalWrite(LED_PINS[i], (state >> i) & 0x01);
    }
}


uint8_t effect = 0;
uint16_t step = 0;
unsigned long lastUpdate = 0;
unsigned long interval = 150;


void setup() {
    for (uint8_t pin : LED_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate < interval) return;
    lastUpdate = currentMillis;

    uint8_t mask = 0;
    const uint16_t maxSteps = 18;

    switch (effect) {
        case 0: // FIFO: Sáng dần 0..7 -> Giữ -> Tắt dần 0..7 -> Giữ
            if (step < 8) {
                interval = 150;
                mask = (1 << (step + 1)) - 1;
            } else if (step == 8) {
                interval = 300;
                mask = 0xFF;
            } else if (step < 17) {
                interval = 150;
                mask = ~((1 << (step - 8)) - 1);
            } else {
                interval = 300;
                mask = 0x00;
            }
            break;

        case 1: // LIFO: Sáng dần 0..7 -> Giữ -> Tắt dần 7..0 -> Giữ
            if (step < 8) {
                interval = 150;
                mask = (1 << (step + 1)) - 1;
            } else if (step == 8) {
                interval = 300;
                mask = 0xFF;
            } else if (step < 17) {
                interval = 150;
                mask = (1 << (16 - step)) - 1;
            } else {
                interval = 300;
                mask = 0x00;
            }
            break;
    }

    setLedByte(mask);

    if (++step >= maxSteps) {
        step = 0;
        effect = (effect + 1) % 2;
    }
}
