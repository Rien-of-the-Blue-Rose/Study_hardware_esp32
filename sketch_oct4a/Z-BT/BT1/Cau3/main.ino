// src/main.cpp
#include <Arduino.h>

const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 1: CẤU HÌNH TIMER NON-BLOCKING VÀ MÁY TRẠNG THÁI (FSM)]
// - Loại bỏ hoàn toàn các hàm delay() gây nghẽn CPU (blocking).
// - Quản lý thời gian bằng bộ đếm millis() kết hợp State Machine phân rã chu kỳ.
// =============================================================================

enum LedModeState {
    FIFO_FILL,      // Sáng dần từ LED 1 đến 8
    FIFO_HOLD_ON,   // Giữ sáng toàn bộ
    FIFO_CLEAR,     // Tắt dần từ LED 1 đến 8
    FIFO_HOLD_OFF,  // Giữ tắt toàn bộ
    LIFO_FILL,      // Sáng dần từ LED 1 đến 8
    LIFO_HOLD_ON,   // Giữ sáng toàn bộ
    LIFO_CLEAR,     // Tắt dần ngược từ LED 8 về 1
    LIFO_HOLD_OFF   // Giữ tắt toàn bộ
};

LedModeState currentState = FIFO_FILL;
int8_t stepIndex = 0;

unsigned long previousMillis = 0;
const unsigned long STEP_INTERVAL_MS = 150; // Chu kỳ bước sáng/tắt từng LED
const unsigned long HOLD_INTERVAL_MS = 300; // Thời gian duy trì trạng thái đầy đủ

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Biến `currentState` và `stepIndex` lưu vết vị trí thực thi của LED thay vì để
     vòng lặp for + delay() chiếm dụng luồng.
   - Phân tách riêng `STEP_INTERVAL_MS` và `HOLD_INTERVAL_MS` để kiểm soát nhịp
     chuyển đổi trạng thái linh hoạt theo mili-giây mà không block core.
   ============================================================================= */

void setup() {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
}

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 2: THUẬT TOÁN ĐIỀU PHỐI KHÔNG CHẶN (NON-BLOCKING ENGINE)]
// - Hàm handleLedSequence() chỉ được gọi khi timer phát hiện đủ khoảng trễ.
// - Xóa bỏ toàn bộ vòng lặp for lồng nhau từ phiên bản cũ.
// =============================================================================

void handleLedSequence() {
    switch (currentState) {
        case FIFO_FILL:
            digitalWrite(LED_PINS[stepIndex], HIGH);
            stepIndex++;
            if (stepIndex >= TOTAL_LEDS) {
                currentState = FIFO_HOLD_ON;
                stepIndex = 0;
            }
            break;

        case FIFO_HOLD_ON:
            currentState = FIFO_CLEAR;
            stepIndex = 0;
            break;

        case FIFO_CLEAR:
            digitalWrite(LED_PINS[stepIndex], LOW);
            stepIndex++;
            if (stepIndex >= TOTAL_LEDS) {
                currentState = FIFO_HOLD_OFF;
                stepIndex = 0;
            }
            break;

        case FIFO_HOLD_OFF:
            currentState = LIFO_FILL;
            stepIndex = 0;
            break;

        case LIFO_FILL:
            digitalWrite(LED_PINS[stepIndex], HIGH);
            stepIndex++;
            if (stepIndex >= TOTAL_LEDS) {
                currentState = LIFO_HOLD_ON;
                stepIndex = TOTAL_LEDS - 1; // Chuẩn bị con trỏ cho pha tắt lùi
            }
            break;

        case LIFO_HOLD_ON:
            currentState = LIFO_CLEAR;
            break;

        case LIFO_CLEAR:
            digitalWrite(LED_PINS[stepIndex], LOW);
            stepIndex--;
            if (stepIndex < 0) {
                currentState = LIFO_HOLD_OFF;
                stepIndex = 0;
            }
            break;

        case LIFO_HOLD_OFF:
            currentState = FIFO_FILL;
            stepIndex = 0;
            break;
    }
}

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Toàn bộ các vòng lặp `for` với `delay(150)` và `delay(300)` cũ đã bị xóa bỏ.
   - Thay thế bằng cấu trúc rẽ nhánh `switch-case`. Mỗi lần timer kích hoạt, chỉ 
     1 thao tác GPIO duy nhất được thực hiện, sau đó trả quyền điều khiển ngay lập tức.
   ============================================================================= */

void loop() {
    unsigned long currentMillis = millis();

    // Xác định ngưỡng thời gian tương ứng với trạng thái giữ hay trạng thái bước dịch
    unsigned long activeInterval = (currentState == FIFO_HOLD_ON  || 
                                   currentState == FIFO_HOLD_OFF || 
                                   currentState == LIFO_HOLD_ON  || 
                                   currentState == LIFO_HOLD_OFF)
                                   ? HOLD_INTERVAL_MS 
                                   : STEP_INTERVAL_MS;

    // Bộ định thời Non-blocking: chỉ nhảy vào xử lý khi thỏa điều kiện chênh lệch thời gian
    if (currentMillis - previousMillis >= activeInterval) {
        previousMillis = currentMillis;
        handleLedSequence();
    }

    // Các tác vụ khác (Serial, cảm biến, Wi-Fi...) có thể đặt ở đây mà không bao giờ bị nghẽn
}

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Vòng lặp `loop()` liên tục quay ở tốc độ tối đa của MCU (~240MHz).
   - Biểu thức `currentMillis - previousMillis >= activeInterval` an toàn tuyệt đối 
     trước hiện tượng tràn số (rollover sau ~49.7 ngày) của hàm millis().
   ============================================================================= */