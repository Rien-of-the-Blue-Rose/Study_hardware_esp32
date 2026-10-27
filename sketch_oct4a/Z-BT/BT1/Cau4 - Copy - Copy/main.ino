// src/main.cpp
#include <Arduino.h>

// Định nghĩa mảng 8 chân GPIO điều khiển LED theo thứ tự từ LED 1 đến LED 8
const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 1: THIẾT LẬP PWM THEO CHUẨN ARDUINO ESP32 CORE V3.X+]
// - Core 3.x+ loại bỏ hoàn toàn cơ chế gán kênh thủ công (ledcSetup, ledcAttachPin).
// - Chuyển sang API mới: ledcAttach(pin, freq, resolution) và ledcWrite(pin, duty).
// =============================================================================

const uint32_t PWM_FREQ = 5000;    // Tần số 5kHz chống nhấp nháy mắt người
const uint8_t  PWM_RES  = 8;       // Độ phân giải 8-bit (Giá trị Duty Cycle từ 0 đến 255)

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Trên ESP32 Arduino Core 3+, kênh PWM (0-15) được driver tự động cấp phát ngầm.
   - Hàm ledcAttach(pin, freq, resolution) gắn trực tiếp cấu hình PWM vào chân GPIO.
   - Thao tác xuất độ sáng dùng trực tiếp chân: ledcWrite(pin, duty).
   ============================================================================= */

// --- CẤU TRÚC ĐIỀU KHIỂN ĐỘ SÁNG NON-BLOCKING (STATE STRUCTS) ---

struct BreathingState {
    unsigned long lastUpdate = 0;
    int16_t duty = 0;
    int8_t step = 3;
    uint8_t cycleCount = 0;
    const unsigned long INTERVAL = 12; // 12ms mỗi bước biến thiên
};

struct WaveState {
    unsigned long lastUpdate = 0;
    uint16_t wavePhase = 0;
    uint16_t stepCount = 0;
    const unsigned long INTERVAL = 20;
};

struct SequentialFadeState {
    unsigned long lastUpdate = 0;
    uint8_t currentLed = 0;
    int16_t duty = 0;
    int8_t step = 5;
    const unsigned long INTERVAL = 8;
};

struct DualPhaseState {
    unsigned long lastUpdate = 0;
    int16_t duty = 0;
    int8_t step = 2;
    uint8_t cycleCount = 0;
    const unsigned long INTERVAL = 15;
};

struct CometState {
    unsigned long lastUpdate = 0;
    int8_t head = 0;
    uint8_t round = 0;
    const unsigned long INTERVAL = 80;
};

// Khởi tạo các biến trạng thái
BreathingState      stateBreathe;
WaveState           stateWave;
SequentialFadeState stateSeqFade;
DualPhaseState      stateDualPhase;
CometState          stateComet;

uint8_t currentBrightnessEffect = 1;

// Hàm phụ trợ
void setAllPwm(uint8_t duty);

// Khai báo 5 hiệu ứng biến đổi độ sáng độc lập (Non-blocking)
bool effect1_breathingAll();
bool effect2_brightnessWave();
bool effect3_sequentialFade();
bool effect4_dualPhaseBreathe();
bool effect5_cometTail();

void setup() {
    // =========================================================================
    // [ĐOẠN THAY ĐỔI NỔI BẬT 2: KHỞI TẠO NGOẠI VI PWM THAY CHO OUTPUT KỸ THUẬT SỐ]
    // =========================================================================
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        // Cú pháp chuẩn ESP32 Core v3.x+
        ledcAttach(LED_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(LED_PINS[i], 0);
    }
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Thay thế pinMode(..., OUTPUT) và digitalWrite(..., LOW) bằng ledcAttach()
     và ledcWrite(..., 0) để khởi tạo chân ở chế độ điều rộng xung PWM 8-bit.
   ============================================================================= */

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 3: BỘ ĐIỀU PHỐI TUẦN TỰ CÁC HIỆU ỨNG ĐỘ SÁNG]
// =============================================================================
void loop() {
    switch (currentBrightnessEffect) {
        case 1:
            if (effect1_breathingAll()) currentBrightnessEffect = 2;
            break;
        case 2:
            if (effect2_brightnessWave()) currentBrightnessEffect = 3;
            break;
        case 3:
            if (effect3_sequentialFade()) currentBrightnessEffect = 4;
            break;
        case 4:
            if (effect4_dualPhaseBreathe()) currentBrightnessEffect = 5;
            break;
        case 5:
            if (effect5_cometTail()) currentBrightnessEffect = 1;
            break;
    }
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Chuyển máy trạng thái FSM sang điều phối 5 giải thuật biến thiên độ sáng PWM
     thay vì các hiệu ứng bật/tắt (ON/OFF) rời rạc của Nội dung 1 & 3.
   ============================================================================= */

void setAllPwm(uint8_t duty) {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        ledcWrite(LED_PINS[i], duty);
    }
}

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 4: CHI TIẾT 5 THUẬT TOÁN BIẾN THIÊN ĐỘ SÁNG PWM]
// =============================================================================

// Hiệu ứng 1: Toàn bộ 8 LED "thở" (Breathing Light) đồng loạt (Chạy 2 chu kỳ)
bool effect1_breathingAll() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateBreathe.lastUpdate >= stateBreathe.INTERVAL) {
        stateBreathe.lastUpdate = currentMillis;

        setAllPwm(stateBreathe.duty);
        stateBreathe.duty += stateBreathe.step;

        // Đảo chiều biến thiên khi chạm ngưỡng 0 hoặc 255
        if (stateBreathe.duty >= 255) {
            stateBreathe.duty = 255;
            stateBreathe.step = -abs(stateBreathe.step);
        } else if (stateBreathe.duty <= 0) {
            stateBreathe.duty = 0;
            stateBreathe.step = abs(stateBreathe.step);
            stateBreathe.cycleCount++;

            if (stateBreathe.cycleCount >= 2) {
                stateBreathe.cycleCount = 0;
                return true; // Kết thúc hiệu ứng
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Độ sáng toàn hàng LED tăng dần mượt mà từ 0 -> 255 (tỏ dần), sau đó giảm từ
     255 -> 0 (mờ dần). Không xảy ra hiện tượng bật/tắt tức thời.
   ============================================================================= */

// Hiệu ứng 2: Sóng độ sáng lan truyền (Traveling Brightness Wave)
bool effect2_brightnessWave() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateWave.lastUpdate >= stateWave.INTERVAL) {
        stateWave.lastUpdate = currentMillis;

        for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
            // Tính toán lệch pha độ sáng tam giác giữa các LED liền kề (bước pha 32 đơn vị)
            uint8_t localPhase = (stateWave.wavePhase + i * 32) & 0xFF;
            uint8_t duty = (localPhase < 128) ? (localPhase * 2) : ((255 - localPhase) * 2);
            ledcWrite(LED_PINS[i], duty);
        }

        stateWave.wavePhase += 6;
        stateWave.stepCount++;

        if (stateWave.stepCount >= 200) { // Chạy quét sóng ~4 giây
            stateWave.stepCount = 0;
            stateWave.wavePhase = 0;
            setAllPwm(0);
            return true;
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Tạo chênh lệch độ sáng liên tục giữa 8 LED bằng giải thuật dịch pha (phase shift),
     tạo cảm giác dải sáng dập dềnh trôi dọc từ LED 1 sang LED 8.
   ============================================================================= */

// Hiệu ứng 3: Từng LED lần lượt tỏ dần rồi mờ dần nối tiếp (Sequential Cascade Fade)
bool effect3_sequentialFade() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateSeqFade.lastUpdate >= stateSeqFade.INTERVAL) {
        stateSeqFade.lastUpdate = currentMillis;

        ledcWrite(LED_PINS[stateSeqFade.currentLed], stateSeqFade.duty);
        stateSeqFade.duty += stateSeqFade.step;

        if (stateSeqFade.duty >= 255) {
            stateSeqFade.duty = 255;
            stateSeqFade.step = -abs(stateSeqFade.step);
        } else if (stateSeqFade.duty <= 0) {
            stateSeqFade.duty = 0;
            stateSeqFade.step = abs(stateSeqFade.step);
            ledcWrite(LED_PINS[stateSeqFade.currentLed], 0);

            stateSeqFade.currentLed++;
            if (stateSeqFade.currentLed >= TOTAL_LEDS) {
                stateSeqFade.currentLed = 0;
                return true; // Hoàn thành quét qua 8 LED
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Thay vì bật tắt lập tức, mỗi LED khi đến lượt sẽ tăng từ từ độ sáng lên đỉnh,
     sau đó giảm êm dịu về 0 rồi mới chuyển quyền điều khiển sang bóng kế tiếp.
   ============================================================================= */

// Hiệu ứng 4: Thở nghịch pha đối xứng giữa 2 cụm LED (Dual-Phase Seesaw Fade)
bool effect4_dualPhaseBreathe() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateDualPhase.lastUpdate >= stateDualPhase.INTERVAL) {
        stateDualPhase.lastUpdate = currentMillis;

        uint8_t dutyLeft = stateDualPhase.duty;
        uint8_t dutyRight = 255 - stateDualPhase.duty;

        // Cụm trái (LED 1 - LED 4)
        for (uint8_t i = 0; i < 4; i++) {
            ledcWrite(LED_PINS[i], dutyLeft);
        }
        // Cụm phải (LED 5 - LED 8)
        for (uint8_t i = 4; i < TOTAL_LEDS; i++) {
            ledcWrite(LED_PINS[i], dutyRight);
        }

        stateDualPhase.duty += stateDualPhase.step;

        if (stateDualPhase.duty >= 255) {
            stateDualPhase.duty = 255;
            stateDualPhase.step = -abs(stateDualPhase.step);
        } else if (stateDualPhase.duty <= 0) {
            stateDualPhase.duty = 0;
            stateDualPhase.step = abs(stateDualPhase.step);
            stateDualPhase.cycleCount++;

            if (stateDualPhase.cycleCount >= 3) {
                stateDualPhase.cycleCount = 0;
                setAllPwm(0);
                return true;
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Phân chia 8 LED thành 2 nửa: Cụm 1..4 nhận duty, cụm 5..8 nhận (255 - duty).
   - Khi cụm trái đạt độ sáng cực đại thì cụm phải tắt hoàn toàn và ngược lại.
   ============================================================================= */

// Hiệu ứng 5: Vệt sáng sao băng với đuôi phân tầng mờ dần (Comet / Meteor Decay)
bool effect5_cometTail() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateComet.lastUpdate >= stateComet.INTERVAL) {
        stateComet.lastUpdate = currentMillis;

        // Xóa sạch trạng thái trước đó
        setAllPwm(0);

        int8_t h = stateComet.head;

        // Thiết lập gradient độ sáng theo khoảng cách sau đầu sao băng (Head)
        if (h >= 0 && h < TOTAL_LEDS)          ledcWrite(LED_PINS[h], 255);    // Điểm đầu: 100% độ sáng
        if ((h - 1) >= 0 && (h - 1) < TOTAL_LEDS) ledcWrite(LED_PINS[h - 1], 90); // Thân vệt: ~35% độ sáng
        if ((h - 2) >= 0 && (h - 2) < TOTAL_LEDS) ledcWrite(LED_PINS[h - 2], 25); // Đuôi vệt: ~10% độ sáng
        if ((h - 3) >= 0 && (h - 3) < TOTAL_LEDS) ledcWrite(LED_PINS[h - 3], 5);  // Tàn dư: ~2% độ sáng

        stateComet.head++;

        // Cho vệt sáng quét trôi ra khỏi biên LED 8
        if (stateComet.head >= TOTAL_LEDS + 3) {
            stateComet.head = 0;
            stateComet.round++;

            if (stateComet.round >= 3) {
                stateComet.round = 0;
                setAllPwm(0);
                return true;
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Sử dụng các mức duty PWM không đồng nhất (255 -> 90 -> 25 -> 5) phân bổ trên
     các LED liền kề để tạo hiệu ứng thị giác vệt đuôi sao băng lướt qua hàng LED.
   ============================================================================= */