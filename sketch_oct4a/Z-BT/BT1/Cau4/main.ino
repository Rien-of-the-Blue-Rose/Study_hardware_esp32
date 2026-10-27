// src/main.cpp
#include <Arduino.h>

const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 1: HẠ TẦN SỐ PWM VÀ TÍCH HỢP HÀM GAMMA CORRECTION]
// - Hạ tần số PWM từ 5000Hz xuống 1000Hz để tương thích cơ chế render của Wokwi.
// - Bổ sung hàm gammaCorrection() chuyển đổi tuyến tính sang phi tuyến (y = x^2 / 255).
// =============================================================================

const uint32_t PWM_FREQ = 1000;    // 1000Hz: Mức tần số chuẩn để Wokwi mô phỏng độ mờ
const uint8_t  PWM_RES  = 8;       // Độ phân giải 8-bit (0 - 255)

// Chuyển đổi mức Duty tuyến tính sang mức sáng phi tuyến để mắt người và Wokwi thấy rõ
static inline uint8_t gammaCorrection(uint8_t inputDuty) {
    return (uint8_t)(((uint16_t)inputDuty * inputDuty) / 255);
}

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Hàm gammaCorrection giúp kéo dài thời gian LED ở dải độ sáng thấp (từ 0 đến 50),
     loại bỏ hiện tượng "LED vừa bật lên đã sáng rực".
   - Tần số 1000Hz giúp bộ giải lập Wokwi hiển thị chính xác độ trong suốt (opacity).
   ============================================================================= */

// --- CẤU TRÚC ĐIỀU KHIỂN ĐỘ SÁNG ĐÃ CÂN CHỈNH LẠI NHỊP THỜI GIAN ---

struct BreathingState {
    unsigned long lastUpdate = 0;
    int16_t duty = 0;
    int8_t step = 2; // Giảm bước nhảy để chuyển tiếp mượt mà
    uint8_t cycleCount = 0;
    const unsigned long INTERVAL = 16; // ~16ms/bước (khớp nhịp 60FPS)
};

struct WaveState {
    unsigned long lastUpdate = 0;
    uint16_t wavePhase = 0;
    uint16_t stepCount = 0;
    const unsigned long INTERVAL = 25;
};

struct SequentialFadeState {
    unsigned long lastUpdate = 0;
    uint8_t currentLed = 0;
    int16_t duty = 0;
    int8_t step = 4;
    const unsigned long INTERVAL = 15;
};

struct DualPhaseState {
    unsigned long lastUpdate = 0;
    int16_t duty = 0;
    int8_t step = 2;
    uint8_t cycleCount = 0;
    const unsigned long INTERVAL = 16;
};

struct CometState {
    unsigned long lastUpdate = 0;
    int8_t head = 0;
    uint8_t round = 0;
    const unsigned long INTERVAL = 120; // Kéo dài để thấy rõ vệt đuôi
};

BreathingState      stateBreathe;
WaveState           stateWave;
SequentialFadeState stateSeqFade;
DualPhaseState      stateDualPhase;
CometState          stateComet;

uint8_t currentBrightnessEffect = 1;

void setAllPwm(uint8_t duty);

bool effect1_breathingAll();
bool effect2_brightnessWave();
bool effect3_sequentialFade();
bool effect4_dualPhaseBreathe();
bool effect5_cometTail();

void setup() {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        ledcAttach(LED_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(LED_PINS[i], 0);
    }
}

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

void setAllPwm(uint8_t duty) {
    uint8_t corrected = gammaCorrection(duty);
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        ledcWrite(LED_PINS[i], corrected);
    }
}

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 2: ÁP DỤNG GAMMA VÀO TOÀN BỘ 5 HIỆU ỨNG ĐỘ SÁNG]
// Mọi giá trị Duty trước khi đẩy vào ledcWrite() đều bắt buộc đi qua gammaCorrection()
// =============================================================================

// Hiệu ứng 1: Thở toàn bộ 8 LED đồng loạt (Rõ rệt từ tối om -> bừng sáng -> tắt dần)
bool effect1_breathingAll() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateBreathe.lastUpdate >= stateBreathe.INTERVAL) {
        stateBreathe.lastUpdate = currentMillis;

        // Xuất qua hàm Gamma
        setAllPwm((uint8_t)stateBreathe.duty);

        stateBreathe.duty += stateBreathe.step;

        if (stateBreathe.duty >= 255) {
            stateBreathe.duty = 255;
            stateBreathe.step = -abs(stateBreathe.step);
        } else if (stateBreathe.duty <= 0) {
            stateBreathe.duty = 0;
            stateBreathe.step = abs(stateBreathe.step);
            stateBreathe.cycleCount++;

            if (stateBreathe.cycleCount >= 2) {
                stateBreathe.cycleCount = 0;
                setAllPwm(0);
                return true;
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Giảm step từ 3 xuống 2 và nâng INTERVAL lên 16ms. Một chu kỳ tăng/giảm kéo dài
     hơn 4 giây giúp Wokwi có đủ khung hình để mô phỏng chuyển tiếp độ sáng từ 0% đến 100%.
   ============================================================================= */

// Hiệu ứng 2: Sóng lan truyền độ sáng (Tương phản cao)
bool effect2_brightnessWave() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateWave.lastUpdate >= stateWave.INTERVAL) {
        stateWave.lastUpdate = currentMillis;

        for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
            uint8_t localPhase = (stateWave.wavePhase + i * 32) & 0xFF;
            uint8_t rawDuty = (localPhase < 128) ? (localPhase * 2) : ((255 - localPhase) * 2);
            
            // Ép qua hàm Gamma để tạo độ tương phản giữa đỉnh sóng và đáy sóng
            ledcWrite(LED_PINS[i], gammaCorrection(rawDuty));
        }

        stateWave.wavePhase += 4; // Giảm tốc độ trôi sóng
        stateWave.stepCount++;

        if (stateWave.stepCount >= 250) {
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
   - Sóng tam giác cũ khi không có Gamma cho ra dải màu bão hòa toàn bộ 8 LED.
   - Nhờ có Gamma, đáy sóng bị kéo sát về 0, giúp thấy rõ 1 đỉnh sáng cô lập di chuyển.
   ============================================================================= */

// Hiệu ứng 3: Tỏ dần rồi mờ dần nối tiếp từng LED (Sequential Cascade Fade)
bool effect3_sequentialFade() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateSeqFade.lastUpdate >= stateSeqFade.INTERVAL) {
        stateSeqFade.lastUpdate = currentMillis;

        ledcWrite(LED_PINS[stateSeqFade.currentLed], gammaCorrection((uint8_t)stateSeqFade.duty));
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
                return true;
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Đảm bảo từng bóng LED tắt hoàn toàn về 0V thực tế trước khi chuyển quyền điều khiển
     sang LED kế tiếp, thấy rõ từng bóng nhú sáng êm dịu rồi chìm hẳn.
   ============================================================================= */

// Hiệu ứng 4: Thở bập bênh nghịch pha giữa 2 cụm (Dual-Phase Seesaw Fade)
bool effect4_dualPhaseBreathe() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateDualPhase.lastUpdate >= stateDualPhase.INTERVAL) {
        stateDualPhase.lastUpdate = currentMillis;

        uint8_t dutyLeft  = gammaCorrection((uint8_t)stateDualPhase.duty);
        uint8_t dutyRight = gammaCorrection((uint8_t)(255 - stateDualPhase.duty));

        for (uint8_t i = 0; i < 4; i++) {
            ledcWrite(LED_PINS[i], dutyLeft);
        }
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
   - Khi không có Gamma, tổng cường độ (Left + Right) luôn tạo cảm giác đèn nào cũng sáng.
   - Khi áp dụng Gamma, điểm giao nhau ở giữa sẽ hạ xuống thấp, phân định ranh giới
     sáng/tối cực kỳ rõ giữa cụm 4 LED bên trái và cụm 4 LED bên phải.
   ============================================================================= */

// Hiệu ứng 5: Vệt sao băng phân tầng mờ dần (Comet Tail)
bool effect5_cometTail() {
    unsigned long currentMillis = millis();
    if (currentMillis - stateComet.lastUpdate >= stateComet.INTERVAL) {
        stateComet.lastUpdate = currentMillis;

        setAllPwm(0);

        int8_t h = stateComet.head;

        // Phân cấp tỷ lệ Duty qua bảng tính toán trực quan:
        // Đầu (100%): 255 -> Gamma: 255
        // Thân (55%): 140 -> Gamma: 76
        // Đuôi (27%): 70  -> Gamma: 19
        // Tàn dư (12%): 30 -> Gamma: 3
        if (h >= 0 && h < TOTAL_LEDS)          ledcWrite(LED_PINS[h], gammaCorrection(255));
        if ((h - 1) >= 0 && (h - 1) < TOTAL_LEDS) ledcWrite(LED_PINS[h - 1], gammaCorrection(140));
        if ((h - 2) >= 0 && (h - 2) < TOTAL_LEDS) ledcWrite(LED_PINS[h - 2], gammaCorrection(70));
        if ((h - 3) >= 0 && (h - 3) < TOTAL_LEDS) ledcWrite(LED_PINS[h - 3], gammaCorrection(30));

        stateComet.head++;

        if (stateComet.head >= TOTAL_LEDS + 4) {
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
   - Điều chỉnh lại các ngưỡng sáng đuôi sao băng kết hợp Gamma.
   - Khoảng trôi INTERVAL nâng lên 120ms giúp mắt người thấy rõ vệt sáng kéo dài
     với độ sáng giảm dần từng nấc phía sau đầu sao băng.
   ============================================================================= */