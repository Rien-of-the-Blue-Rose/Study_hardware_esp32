// src/main.cpp
#include <Arduino.h>

// Định nghĩa mảng 8 chân GPIO điều khiển LED theo thứ tự từ LED 1 đến LED 8
const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 1: CẤU TRÚC LƯU TRỮ TRẠNG THÁI NON-BLOCKING (STATE STRUCTS)]
// Chuẩn hóa và bổ sung đầy đủ biến cờ (flags), bước đếm (step/phase) cho từng hiệu ứng.
// Mỗi struct đóng gói độc lập chu kỳ (INTERVAL) và mốc thời gian (lastUpdate).
// =============================================================================

struct BlinkState {
    unsigned long lastUpdate = 0;
    uint8_t cycleCount = 0;
    bool isOn = false;
    const unsigned long INTERVAL = 250;
};

struct SequenceState {
    unsigned long lastUpdate = 0;
    int8_t index = 0;
    uint8_t phase = 0; // 0: Sáng dần, 1: Tắt dần
    const unsigned long INTERVAL = 100;
};

struct KnightRiderState {
    unsigned long lastUpdate = 0;
    int8_t index = 0;
    uint8_t round = 0;
    bool direction = true; // true = tiến (0 -> 7), false = lùi (6 -> 1)
    const unsigned long INTERVAL = 70;
};

struct FillState {
    unsigned long lastUpdate = 0;
    int8_t limit = 0;
    int8_t pos = TOTAL_LEDS - 1;
    uint8_t fixedMask = 0;
    unsigned long pauseTime = 0;
    bool isPaused = false;
    const unsigned long INTERVAL = 60;
    const unsigned long PAUSE_DURATION = 300;
};

struct CenterState {
    unsigned long lastUpdate = 0;
    int8_t step = 0;
    uint8_t phase = 0; // 0: Mở rộng, 1: Nghỉ giữa, 2: Thu hẹp
    unsigned long pauseTime = 0;
    const unsigned long INTERVAL = 120;
    const unsigned long PAUSE_DURATION = 100;
};

struct AlternateState {
    unsigned long lastUpdate = 0;
    uint8_t cycleCount = 0;
    bool isPattern1 = true;
    const unsigned long INTERVAL = 200;
};

struct CounterState {
    unsigned long lastUpdate = 0;
    uint8_t count = 0;
    const unsigned long INTERVAL = 50;
};

// Khởi tạo thực thể trạng thái toàn cục
BlinkState state1;
SequenceState state2;
SequenceState state3;
KnightRiderState state4;
FillState state5;
CenterState state6;
AlternateState state7;
CounterState state8;

uint8_t currentEffectIndex = 1; // Điều phối tuần tự các hiệu ứng từ 1 đến 8

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Bổ sung trường `phase` cho SequenceState và CenterState để phân định trạng
     thái sáng/tắt/nghỉ mà không cần dùng nhiều cờ rời rạc.
   - Thêm biến điều phối trung tâm `currentEffectIndex` để chạy tuần tự từng hiệu
     ứng mà không làm nghẽn CPU hoặc gây xung đột chân GPIO.
   ============================================================================= */

// Khai báo hàm phụ trợ
void turnOffAll();
void turnOnAll();
void setLedByte(uint8_t state);

// Khai báo 8 hàm hiệu ứng trả về bool (true = đã hoàn thành chu kỳ hiệu ứng)
bool effect1_allBlink();
bool effect2_leftToRight();
bool effect3_rightToLeft();
bool effect4_knightRider();
bool effect5_fillRightToLeft();
bool effect6_centerToOutside();
bool effect7_alternateBlink();
bool effect8_binaryCounter();

void setup() {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
}

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 2: BỘ ĐIỀU PHỐI VÒNG LẶP TUẦN TỰ (FSM SCHEDULER)]
// Xóa bỏ cách gọi 8 hàm đồng thời gây xung đột GPIO.
// Chỉ thực thi hiệu ứng tiếp theo khi hiệu ứng trước đó trả về `true` (hoàn tất).
// =============================================================================

void loop() {
    switch (currentEffectIndex) {
        case 1:
            if (effect1_allBlink()) currentEffectIndex = 2;
            break;
        case 2:
            if (effect2_leftToRight()) currentEffectIndex = 3;
            break;
        case 3:
            if (effect3_rightToLeft()) currentEffectIndex = 4;
            break;
        case 4:
            if (effect4_knightRider()) currentEffectIndex = 5;
            break;
        case 5:
            if (effect5_fillRightToLeft()) currentEffectIndex = 6;
            break;
        case 6:
            if (effect6_centerToOutside()) currentEffectIndex = 7;
            break;
        case 7:
            if (effect7_alternateBlink()) currentEffectIndex = 8;
            break;
        case 8:
            if (effect8_binaryCounter()) currentEffectIndex = 1;
            break;
    }
}

/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Trong code cũ, nếu chỉ xóa delay() mà giữ nguyên gọi 8 hàm trong loop(), 
     cả 8 hiệu ứng sẽ can thiệp vào chân GPIO cùng lúc trong 1 mili-giây.
   - Cấu trúc `switch(currentEffectIndex)` đảm bảo tính tuần tự nguyên bản nhưng 
     hoàn toàn không chiếm dụng vi điều khiển (Non-blocking Cooperative Scheduling).
   ============================================================================= */

// --- HÀM PHỤ TRỢ ---

void turnOffAll() {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        digitalWrite(LED_PINS[i], LOW);
    }
}

void turnOnAll() {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        digitalWrite(LED_PINS[i], HIGH);
    }
}

void setLedByte(uint8_t state) {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        digitalWrite(LED_PINS[i], (state >> i) & 0x01);
    }
}

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 3: CHUYỂN TOÀN BỘ 8 HIỆU ỨNG SANG NON-BLOCKING MILLIS()]
// Loại bỏ 100% vòng lặp for lồng nhau và các lệnh delay().
// =============================================================================

// Hiệu ứng 1: Chớp tắt toàn bộ 8 LED (3 chu kỳ)
bool effect1_allBlink() {
    unsigned long currentMillis = millis();
    if (currentMillis - state1.lastUpdate >= state1.INTERVAL) {
        state1.lastUpdate = currentMillis;

        if (state1.isOn) {
            turnOffAll();
            state1.isOn = false;
            state1.cycleCount++;
            if (state1.cycleCount >= 3) {
                state1.cycleCount = 0;
                state1.isOn = false;
                return true; // Kết thúc hiệu ứng
            }
        } else {
            turnOnAll();
            state1.isOn = true;
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Thay thế vòng lặp `for (uint8_t i = 0; i < 3; i++)` và 2 hàm `delay(250)`.
   - Mỗi lần timer đạt 250ms, hàm đảo cờ `isOn` và cập nhật phần cứng. Khi đủ 3
     chu kỳ (Bật + Tắt), hàm reset biến đếm và báo cờ hoàn tất.
   ============================================================================= */

// Hiệu ứng 2: Sáng dần rồi tắt dần từ LED 1 đến LED 8
bool effect2_leftToRight() {
    unsigned long currentMillis = millis();
    if (currentMillis - state2.lastUpdate >= state2.INTERVAL) {
        state2.lastUpdate = currentMillis;

        if (state2.phase == 0) { // Pha sáng dần
            digitalWrite(LED_PINS[state2.index], HIGH);
            state2.index++;
            if (state2.index >= TOTAL_LEDS) {
                state2.phase = 1;
                state2.index = 0;
            }
        } else { // Pha tắt dần
            digitalWrite(LED_PINS[state2.index], LOW);
            state2.index++;
            if (state2.index >= TOTAL_LEDS) {
                state2.phase = 0;
                state2.index = 0;
                return true; // Kết thúc hiệu ứng
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Thay 2 vòng for tuần tự chứa `delay(100)` bằng máy trạng thái 2 pha (Phase 0: Sáng dần,
     Phase 1: Tắt dần). Con trỏ `state2.index` dịch chuyển mỗi 100ms.
   ============================================================================= */

// Hiệu ứng 3: Sáng dần rồi tắt dần từ LED 8 về LED 1
bool effect3_rightToLeft() {
    unsigned long currentMillis = millis();
    if (currentMillis - state3.lastUpdate >= state3.INTERVAL) {
        state3.lastUpdate = currentMillis;

        if (state3.phase == 0) { // Pha sáng dần từ LED 8 về LED 1
            digitalWrite(LED_PINS[state3.index], HIGH);
            state3.index--;
            if (state3.index < 0) {
                state3.phase = 1;
                state3.index = TOTAL_LEDS - 1;
            }
        } else { // Pha tắt dần từ LED 8 về LED 1
            digitalWrite(LED_PINS[state3.index], LOW);
            state3.index--;
            if (state3.index < 0) {
                state3.phase = 0;
                state3.index = TOTAL_LEDS - 1;
                return true; // Kết thúc hiệu ứng
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Dùng chỉ số `int8_t index` giảm dần từ 7 về 0. Khắc phục hoàn toàn việc chặn
     CPU của `delay(100)` ở code cũ.
   ============================================================================= */

// Hiệu ứng 4: Điểm sáng chạy qua lại (Knight Rider / Scanner)
bool effect4_knightRider() {
    unsigned long currentMillis = millis();
    if (currentMillis - state4.lastUpdate >= state4.INTERVAL) {
        state4.lastUpdate = currentMillis;

        turnOffAll();
        digitalWrite(LED_PINS[state4.index], HIGH);

        if (state4.direction) { // Chiều tiến: 0 -> 7
            state4.index++;
            if (state4.index >= TOTAL_LEDS) {
                state4.direction = false;
                state4.index = TOTAL_LEDS - 2; // Vị trí 6
            }
        } else { // Chiều lùi: 6 -> 1
            state4.index--;
            if (state4.index <= 0) {
                state4.direction = true;
                state4.index = 0;
                state4.round++;
                if (state4.round >= 2) { // Đủ 2 vòng quét
                    state4.round = 0;
                    turnOffAll();
                    return true;
                }
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Loại bỏ 2 vòng lặp lồng nhau chạy trễ 70ms. 
   - Kiểm soát biến đổi vị trí bằng cờ đổi chiều `direction` và đếm vòng `round`.
   ============================================================================= */

// Hiệu ứng 5: Sáng dồn từ phải sang trái (LED 8 -> LED 1)
bool effect5_fillRightToLeft() {
    unsigned long currentMillis = millis();

    // Xử lý khoảng dừng 300ms sau khi đã lấp đầy 8 LED
    if (state5.isPaused) {
        if (currentMillis - state5.pauseTime >= state5.PAUSE_DURATION) {
            state5.isPaused = false;
            state5.limit = 0;
            state5.pos = TOTAL_LEDS - 1;
            state5.fixedMask = 0;
            turnOffAll();
            return true; // Kết thúc hiệu ứng
        }
        return false;
    }

    if (currentMillis - state5.lastUpdate >= state5.INTERVAL) {
        state5.lastUpdate = currentMillis;

        setLedByte(state5.fixedMask | (1 << state5.pos));
        state5.pos--;

        if (state5.pos < state5.limit) {
            state5.fixedMask |= (1 << state5.limit);
            state5.limit++;
            state5.pos = TOTAL_LEDS - 1;

            if (state5.limit >= TOTAL_LEDS) {
                state5.isPaused = true;
                state5.pauseTime = currentMillis;
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Loại bỏ hoàn toàn vòng lặp 2 tầng `for(limit) { for(pos) }` gây treo 
     CPU hàng giây ở phiên bản trước.
   - Trạng thái dừng 300ms (`delay(300)`) được tách thành trạng thái nghỉ `isPaused`
     sử dụng mốc thời gian `pauseTime`.
   ============================================================================= */

// Hiệu ứng 6: Sáng lan từ giữa ra hai biên, sau đó thu dần về giữa
bool effect6_centerToOutside() {
    unsigned long currentMillis = millis();

    if (state6.phase == 1) { // Giai đoạn nghỉ 100ms giữa 2 chu trình
        if (currentMillis - state6.pauseTime >= state6.PAUSE_DURATION) {
            state6.phase = 2;
            state6.step = 0;
        }
        return false;
    }

    if (currentMillis - state6.lastUpdate >= state6.INTERVAL) {
        state6.lastUpdate = currentMillis;

        if (state6.phase == 0) { // Mở rộng sáng từ giữa (cặp 3-4 ra 0-7)
            digitalWrite(LED_PINS[3 - state6.step], HIGH);
            digitalWrite(LED_PINS[4 + state6.step], HIGH);
            state6.step++;
            if (state6.step >= 4) {
                state6.phase = 1;
                state6.pauseTime = currentMillis;
            }
        } else if (state6.phase == 2) { // Thu hẹp tắt từ ngoài biên về giữa
            digitalWrite(LED_PINS[state6.step], LOW);
            digitalWrite(LED_PINS[7 - state6.step], LOW);
            state6.step++;
            if (state6.step >= 4) {
                state6.phase = 0;
                state6.step = 0;
                return true; // Kết thúc hiệu ứng
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Chuyển đổi 2 vòng lặp delay 120ms và quãng nghỉ `delay(100)` thành máy trạng 
     thái 3 pha: Mở rộng (Phase 0) -> Nghỉ (Phase 1) -> Thu hẹp (Phase 2).
   ============================================================================= */

// Hiệu ứng 7: Chớp tắt xen kẽ LED chẵn / lẻ (4 chu kỳ)
bool effect7_alternateBlink() {
    unsigned long currentMillis = millis();
    if (currentMillis - state7.lastUpdate >= state7.INTERVAL) {
        state7.lastUpdate = currentMillis;

        if (state7.isPattern1) {
            setLedByte(0b01010101); // LED 1, 3, 5, 7 sáng
            state7.isPattern1 = false;
        } else {
            setLedByte(0b10101010); // LED 2, 4, 6, 8 sáng
            state7.isPattern1 = true;
            state7.cycleCount++;
            if (state7.cycleCount >= 4) {
                state7.cycleCount = 0;
                state7.isPattern1 = true;
                turnOffAll();
                return true; // Kết thúc hiệu ứng
            }
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Thay thế vòng lặp 4 bước chứa 2 lệnh `delay(200)` bằng việc hoán đổi cờ 
     `isPattern1` sau mỗi 200ms không nghẽn luồng.
   ============================================================================= */

// Hiệu ứng 8: Đếm nhị phân 8-bit (0 -> 63)
bool effect8_binaryCounter() {
    unsigned long currentMillis = millis();
    if (currentMillis - state8.lastUpdate >= state8.INTERVAL) {
        state8.lastUpdate = currentMillis;

        setLedByte(state8.count);
        state8.count++;

        if (state8.count >= 64) {
            state8.count = 0;
            turnOffAll();
            return true; // Kết thúc hiệu ứng
        }
    }
    return false;
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Loại bỏ hoàn toàn vòng lặp `for (count = 0; count < 64)` và lệnh `delay(50)`.
   - Mỗi chu kỳ 50ms tăng 1 giá trị nhị phân, xuất trực tiếp ra phần cứng và trả luồng.
   ============================================================================= */