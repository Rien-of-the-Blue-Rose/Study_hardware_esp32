// src/main.cpp
#include <Arduino.h>

const uint8_t BUTTON_PIN = 0;

const unsigned long DEBOUNCE_MS = 20;
uint32_t count = 0;

uint8_t button_buffer = HIGH;
uint8_t button_stable = HIGH;
unsigned long t = 0;
void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("\n[SYSTEM] ESP32 Button Counter Ready.");
}

void loop() {
    uint8_t button_now = digitalRead(BUTTON_PIN);
    // 1. Ghi nhận biến thiên cơ học để reset bộ đếm thời gian lọc nhiễu
    if (button_now != button_buffer) {
        t = millis();
        button_buffer = button_now;
    }

    // 2. Tín hiệu vượt ngưỡng ổn định (20ms) VÀ khác trạng thái chốt trước đó
    if ((millis() - t) >= DEBOUNCE_MS) {
        if (button_buffer != button_stable) {
            button_stable = button_buffer;

            // 3. Chỉ kích hoạt khi sườn tín hiệu rơi xuống mức LOW (nhấn nút)
            if (button_stable == LOW) {
                count = count + 1;
                Serial.printf("[NHAN LAN %u] Gia tri dem hien tai: %u\n", count, count);
            }
        }
    }
}
