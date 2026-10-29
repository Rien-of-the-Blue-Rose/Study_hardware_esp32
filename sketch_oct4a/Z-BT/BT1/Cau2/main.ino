// src/main.cpp
#include <Arduino.h>

const uint8_t BUTTON_PIN = 0;
const unsigned long DEBOUNCE_MS = 50;
uint32_t pressCount = 0;

uint8_t lastRawState = HIGH;
uint8_t stableState = HIGH;
unsigned long lastDebounceTime = 0;

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("\n[SYSTEM] ESP32 Button Counter Ready.");
}

void loop() {
    uint8_t reading = digitalRead(BUTTON_PIN);
    unsigned long now = millis();

    // 1. Ghi nhận biến thiên cơ học để reset bộ đếm thời gian lọc nhiễu
    if (reading != lastRawState) {
        lastDebounceTime = now;
        lastRawState = reading;
    }

    // 2. Tín hiệu vượt ngưỡng ổn định VÀ khác trạng thái chốt trước đó
    if ((now - lastDebounceTime >= DEBOUNCE_MS) && (reading != stableState)) {
        stableState = reading;
        // 3. Chỉ kích hoạt khi sườn tín hiệu rơi xuống mức LOW (nhấn nút)
        if (stableState == LOW) {
            pressCount++;
            Serial.printf("[NHAN LAN %u] Gia tri dem hien tai: %u\n", pressCount, pressCount);
        }
    }
}
