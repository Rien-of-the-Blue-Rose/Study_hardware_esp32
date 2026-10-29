// src/main.cpp
#include <Arduino.h>

const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

void setLedByte(uint8_t state) {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        digitalWrite(LED_PINS[i], (state >> i) & 0x01);
    }
}

uint8_t effect = 0;
uint16_t step = 0;
unsigned long lastUpdate = 0;
unsigned long interval = 250;

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
    uint16_t maxSteps = 0;

    switch (effect) {
        case 0: // Chớp tắt 3 lần (6 nấc)
            interval = 250;
            mask = (step % 2 == 0) ? 0xFF : 0x00;
            maxSteps = 6;
            break;

        case 1: // Sáng/tắt dần Trái -> Phải (16 nấc)
            interval = 100;
            mask = (step < 8) ? ((1 << (step + 1)) - 1) : ~((1 << (step - 7)) - 1);
            maxSteps = 16;
            break;

        case 2: // Sáng/tắt dần Phải -> Trái (16 nấc)
            interval = 100;
            mask = (step < 8) ? (0xFF << (7 - step)) : ~(0xFF << (15 - step));
            maxSteps = 16;
            break;

        case 3: { // Knight Rider (28 nấc)
            interval = 70;
            uint8_t p = step % 14;
            mask = 1 << (p <= 7 ? p : 14 - p);
            maxSteps = 28;
            break;
        }

        case 4: { // Sáng dồn Phải -> Trái (37 nấc)
            maxSteps = 37;
            if (step == 36) {
                interval = 300; // Dừng lại 300ms ngắm kết quả sáng full
                mask = 0xFF;
            } else {
                interval = 60;
                uint8_t lim = 0, acc = 0;
                while (acc + 8 - lim <= step) acc += 8 - lim++;
                // Triệt tiêu toán tử 3 ngôi phòng thủ thừa: khi lim = 0 thì (1 << 0) - 1 == 0
                mask = ((1 << lim) - 1) | (1 << (7 - step + acc));
            }
            break;
        }

        case 5: { // Lan tỏa từ tâm ra biên và thu hẹp (9 nấc)
            maxSteps = 9;
            if (step <= 3) {
                interval = 120;
                mask = (0x18 << step) | (0x18 >> step);
            } else if (step == 4) {
                interval = 100; // Dừng nghỉ 100ms ở 2 biên
                mask = 0xC3;
            } else {
                interval = 120;
                uint8_t m = (1 << (step - 4)) - 1;
                mask = ~(m | (m << (12 - step)));
            }
            break;
        }

        case 6: // Chớp tắt xen kẽ (8 nấc)
            interval = 200;
            mask = (step % 2 == 0) ? 0x55 : 0xAA;
            maxSteps = 8;
            break;

        case 7: // Đếm nhị phân 0..63 (64 nấc)
            interval = 50;
            mask = step;
            maxSteps = 64;
            break;
    }

    // Cập nhật phần cứng duy nhất một lần ở mỗi chu kỳ
    setLedByte(mask);

    // Bộ điều phối chuyển tầng tập trung: Tự động đảo chu kỳ khi chạm trần maxSteps
    if (++step >= maxSteps) {
        step = 0;
        effect = (effect + 1) % 8;
    }
}
