// src/main.cpp
#include <Arduino.h>

const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

const uint32_t PWM_FREQ = 5000;
const uint8_t  PWM_RES  = 8; // Độ phân giải 8-bit (0 - 255)

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 1: CẤU TRÚC ĐIỀU KHIỂN DẢI GRADIENT ĐỐI XỨNG LỆCH PHA]
// - Thiết lập 2 chế độ biến đổi gradient động:
//   1. Seesaw Gradient (Bập bênh tuyến tính): LED 1 max (255) -> LED 8 min (0), 
//      sau đó nghiêng dốc ngược lại LED 1 min -> LED 8 max.
//   2. Rotating Gradient Wave (Sóng dốc quay vòng): Cực đại và cực tiểu luôn 
//      cách nhau 180 độ (4 bước chân LED), dịch chuyển liên tục qua hàng LED.
// =============================================================================

struct GradientSeesawState {
    unsigned long lastUpdate = 0;
    int16_t headDuty = 255;  // Độ sáng của LED 1 (0 -> 255)
    int8_t step = -2;        // Tốc độ biến thiên dốc
    uint8_t cycleCount = 0;
    const unsigned long INTERVAL = 12; // 12ms mỗi bước nội suy
};

struct GradientWaveState {
    unsigned long lastUpdate = 0;
    uint16_t waveAngle = 0;  // Góc pha quét (0 -> 359 độ)
    uint16_t stepCount = 0;
    const unsigned long INTERVAL = 15;
};

GradientSeesawState stateSeesaw;
GradientWaveState   stateWave;

uint8_t currentMode = 1;

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Thay thế toàn bộ 5 hiệu ứng PWM cũ bằng 2 cấu trúc Gradient động tập trung 
     vào nguyên lý chênh lệch độ sáng đối đỉnh giữa LED 1 và LED 8.
   - Chu kỳ cập nhật 12-15ms đảm bảo tần số quét trên 60 FPS, mắt người không 
     thể nhận thấy độ giật khi dải sáng trôi.
   ============================================================================= */

// Khai báo hàm
bool runGradientSeesaw();
bool runRotatingGradientWave();

void setup() {
    // Khởi tạo PWM Core v3.x+ cho toàn bộ 8 chân
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        ledcAttach(LED_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(LED_PINS[i], 0);
    }
}

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 2: BỘ ĐIỀU PHỐI NON-BLOCKING FSM]
// =============================================================================
void loop() {
    switch (currentMode) {
        case 1:
            if (runGradientSeesaw()) currentMode = 2;
            break;
        case 2:
            if (runRotatingGradientWave()) currentMode = 1;
            break;
    }
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Vòng lặp loop() hoạt động tuần tự nhưng hoàn toàn giải phóng CPU (non-blocking).
   - Chuyển giao mượt mà giữa hiệu ứng bập bênh dải sáng và sóng dốc tuần hoàn.
   ============================================================================= */

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 3: THUẬT TOÁN NỘI SUY TUYẾN TÍNH GRADIENT BẬP BÊNH]
// Công thức: Duty(i) = Head + i * (Tail - Head) / 7
// Khi LED 1 = 255 thì LED 8 = 0. Các LED ở giữa nhận giá trị giảm dần bậc thang.
// =============================================================================

bool runGradientSeesaw() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateSeesaw.lastUpdate >= stateSeesaw.INTERVAL) {
        stateSeesaw.lastUpdate = currentMillis;

        int16_t duty1 = stateSeesaw.headDuty;       // Độ sáng LED 1
        int16_t duty8 = 255 - stateSeesaw.headDuty; // Độ sáng LED 8 (nghịch đảo tuyệt đối)

        // Phân phối gradient tuyến tính dọc theo 8 LED
        for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
            int16_t dutyI = duty1 + (int32_t)(i * (duty8 - duty1)) / (TOTAL_LEDS - 1);
            ledcWrite(LED_PINS[i], (uint8_t)constrain(dutyI, 0, 255));
        }

        stateSeesaw.headDuty += stateSeesaw.step;

        // Đảo chiều dốc khi chạm 2 ngưỡng cực trị
        if (stateSeesaw.headDuty <= 0) {
            stateSeesaw.headDuty = 0;
            stateSeesaw.step = abs(stateSeesaw.step);
        } else if (stateSeesaw.headDuty >= 255) {
            stateSeesaw.headDuty = 255;
            stateSeesaw.step = -abs(stateSeesaw.step);
            stateSeesaw.cycleCount++;

            if (stateSeesaw.cycleCount >= 3) { // Chạy 3 chu kỳ hoàn chỉnh
                stateSeesaw.cycleCount = 0;
                return true;
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Toán tử nội suy số nguyên `duty1 + (i * (duty8 - duty1)) / 7` đảm bảo tính toán 
     nhẹ nhất cho MCU, triệt tiêu việc dùng số thực float.
   - Tại đỉnh: LED 1 = 255, LED 2 ≈ 219, LED 3 ≈ 182, LED 4 ≈ 146, 
     LED 5 ≈ 109, LED 6 ≈ 73, LED 7 ≈ 36, LED 8 = 0.
   ============================================================================= */

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 4: THUẬT TOÁN SÓNG DỐC TUẦN HOÀN (ROTATING GRADIENT)]
// Dịch pha tuần hoàn liên tục theo góc 360 độ. Khoảng cách pha giữa LED 1 và 
// LED 8 luôn bằng 180 độ (π rad) -> Luôn đảm bảo: LED 1 MAX thì LED 8 MIN.
// =============================================================================

bool runRotatingGradientWave() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateWave.lastUpdate >= stateWave.INTERVAL) {
        stateWave.lastUpdate = currentMillis;

        for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
            // Mỗi LED lệch nhau đúng (180 độ / 7) ≈ 25.7 độ
            // Do đó LED 0 (LED 1) và LED 7 (LED 8) luôn lệch nhau chính xác 180 độ
            int32_t phase = (stateWave.waveAngle + (i * 180) / (TOTAL_LEDS - 1)) % 360;

            // Chuyển góc pha thành tam giác sóng đối xứng (0 -> 255 -> 0)
            int16_t duty;
            if (phase < 180) {
                duty = (phase * 255) / 180;
            } else {
                duty = ((360 - phase) * 255) / 180;
            }

            ledcWrite(LED_PINS[i], (uint8_t)constrain(duty, 0, 255));
        }

        stateWave.waveAngle = (stateWave.waveAngle + 3) % 360;
        stateWave.stepCount++;

        if (stateWave.stepCount >= 360) { // Quét đủ 3 vòng tuần hoàn
            stateWave.stepCount = 0;
            stateWave.waveAngle = 0;
            return true;
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Góc pha giữa LED i và LED i+1 được khóa cứng theo bước `180 / 7`. 
   - Khi `phase(LED 1) = 90°` (đạt đỉnh 255) thì `phase(LED 8) = 270°` (đáy 0).
   - Hiệu ứng tạo ra dải ánh sáng chuyển màu mượt mà, trôi liên tục nhưng luôn 
     giữ nguyên mối quan hệ đối đỉnh cực đại - cực tiểu giữa 2 đầu.
   ============================================================================= */