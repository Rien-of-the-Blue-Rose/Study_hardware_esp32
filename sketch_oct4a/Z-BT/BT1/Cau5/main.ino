// src/main.cpp
#include <Arduino.h>

// =============================================================================
// [CẤU HÌNH PHẦN CỨNG NỘI DUNG 5]
// - LED mục tiêu: GPIO 2 (LED thứ 7 trên dãy 8 LED của bài 1)
// - Phím 1 (TĂNG ĐỘ SÁNG): GPIO 0 (Nút Boot tích hợp sẵn trên ESP32)
// - Phím 2 (GIẢM ĐỘ SÁNG): GPIO 25 (Nút nhấn rời lắp thêm, chế độ INPUT_PULLUP)
// =============================================================================

const uint8_t LED_TARGET_PIN = 2; // Chân điều khiển LED thay đổi độ sáng
const uint8_t BTN_UP_PIN     = 0; // Phím Boot: Tăng độ sáng
const uint8_t BTN_DOWN_PIN   = 25;// Phím lắp thêm: Giảm độ sáng

// Danh sách các chân LED còn lại để tắt triệt để, tránh sáng ngầm
const uint8_t OTHER_LEDS[] = {19, 18, 5, 17, 16, 4, 15};
const uint8_t NUM_OTHER_LEDS = sizeof(OTHER_LEDS) / sizeof(OTHER_LEDS[0]);

// =============================================================================
// [CẤU HÌNH PWM VÀ THANG ĐỘ SÁNG PHI TUYẾN (ESP32 CORE 3.X+)]
// =============================================================================
const uint32_t PWM_FREQ = 1000; // 1000Hz tối ưu mô phỏng Wokwi
const uint8_t  PWM_RES  = 8;    // Độ phân giải 8-bit (0 - 255)

const uint8_t MAX_LEVEL = 10;   // Thang chia 10 nấc độ sáng (Mức 0 = Tắt, Mức 10 = Cực đại)
uint8_t brightnessLevel = 0;    // Khởi tạo ban đầu ở mức 0 (Tắt)

// Bảng tra Duty Cycle kết hợp Gamma Correction chuẩn hóa cho 10 nấc độ sáng
// Giúp mắt người và đồ họa Wokwi thấy từng nấc biến thiên rõ rệt:
const uint8_t GAMMA_LEVELS[MAX_LEVEL + 1] = {
    0,   // Mức 0: Tắt hoàn toàn (0%)
    3,   // Mức 1: Nhú sáng mờ (1.2%)
    8,   // Mức 2: Sáng nhẹ (3.1%)
    16,  // Mức 3: (6.3%)
    28,  // Mức 4: (11.0%)
    45,  // Mức 5: Sáng trung bình (17.6%)
    72,  // Mức 6: (28.2%)
    110, // Mức 7: (43.1%)
    155, // Mức 8: (60.8%)
    205, // Mức 9: (80.4%)
    255  // Mức 10: Sáng cực đại (100%)
};

// =============================================================================
// [CẤU TRÚC MÁY TRẠNG THÁI CHỐNG DỘI PHÍM ĐỘC LẬP (DUAL FSM DEBOUNCE)]
// =============================================================================
enum ButtonFsmState {
    BTN_STATE_IDLE,
    BTN_STATE_DEBOUNCE_PRESS,
    BTN_STATE_HELD,
    BTN_STATE_DEBOUNCE_RELEASE
};

struct DebouncedButton {
    uint8_t pin;
    ButtonFsmState state;
    unsigned long timer;
    const unsigned long DEBOUNCE_MS = 50; // 50ms chống dội
};

DebouncedButton btnUp   = {BTN_UP_PIN,   BTN_STATE_IDLE, 0};
DebouncedButton btnDown = {BTN_DOWN_PIN, BTN_STATE_IDLE, 0};

// Cập nhật độ sáng ra phần cứng và in trạng thái ra Serial Monitor
void applyBrightness(const char* actionReason) {
    uint8_t duty = GAMMA_LEVELS[brightnessLevel];
    ledcWrite(LED_TARGET_PIN, duty);

    Serial.printf("[SU KIEN: %s] -> Nac: %u/%u | Duty PWM: %3u/255 | Cong suat: %3u%%\n",
                  actionReason,
                  brightnessLevel,
                  MAX_LEVEL,
                  duty,
                  (brightnessLevel * 100) / MAX_LEVEL);
}

// Hàm quét nút nhấn chống dội FSM: Trả về true CHỈ 1 LẦN duy nhất khi nhấn phím thành công
bool checkButtonPress(DebouncedButton &btn) {
    int pinVal = digitalRead(btn.pin);
    unsigned long currentMillis = millis();

    switch (btn.state) {
        case BTN_STATE_IDLE:
            if (pinVal == LOW) { // Phát hiện bắt đầu chạm phím
                btn.timer = currentMillis;
                btn.state = BTN_STATE_DEBOUNCE_PRESS;
            }
            break;

        case BTN_STATE_DEBOUNCE_PRESS:
            if (pinVal == HIGH) {
                btn.state = BTN_STATE_IDLE; // Nhiễu nảy tiếp điểm < 50ms
            } else if (currentMillis - btn.timer >= btn.DEBOUNCE_MS) {
                btn.state = BTN_STATE_HELD;
                return true; // XÁC NHẬN NHẤN THÀNH CÔNG (CHỈ TRẢ VỀ TRUE ĐÚNG 1 LẦN)
            }
            break;

        case BTN_STATE_HELD:
            if (pinVal == HIGH) { // Bắt đầu nhả phím
                btn.timer = currentMillis;
                btn.state = BTN_STATE_DEBOUNCE_RELEASE;
            }
            break;

        case BTN_STATE_DEBOUNCE_RELEASE:
            if (pinVal == LOW) {
                btn.timer = currentMillis;
            } else if (currentMillis - btn.timer >= btn.DEBOUNCE_MS) {
                btn.state = BTN_STATE_IDLE; // Đã nhả tay hoàn toàn, sẵn sàng cho lần bấm sau
            }
            break;
    }
    return false;
}

void setup() {
    Serial.begin(115200);

    // Cấu hình 2 phím bấm sử dụng điện trở kéo lên nội bộ (Active-LOW)
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

    // Tắt toàn bộ các LED không dùng đến
    for (uint8_t i = 0; i < NUM_OTHER_LEDS; i++) {
        pinMode(OTHER_LEDS[i], OUTPUT);
        digitalWrite(OTHER_LEDS[i], LOW);
    }

    // Cấu hình LEDC PWM chân GPIO 2 theo chuẩn Core v3.x+
    ledcAttach(LED_TARGET_PIN, PWM_FREQ, PWM_RES);
    applyBrightness("KHOI TAO");

    delay(300);
    Serial.println("==================================================");
    Serial.println("    ESP32 CORE 3+ NDS: HE THONG DIEU KHIEN DO SANG");
    Serial.println(" - Phim Boot (GPIO 0) : TANG do sang 1 nac       ");
    Serial.println(" - Phim ngoai (GPIO 25): GIAM do sang 1 nac       ");
    Serial.println("==================================================");
}

void loop() {
    // 1. Quét phím TĂNG (GPIO 0 - Boot)
    if (checkButtonPress(btnUp)) {
        if (brightnessLevel < MAX_LEVEL) {
            brightnessLevel++;
            applyBrightness("PHIM BOOT (TANG)");
        } else {
            Serial.println("[CANH BAO] Do sang da dat muc TOI DA (10/10)!");
        }
    }

    // 2. Quét phím GIẢM (GPIO 25 - Nút gắn thêm)
    if (checkButtonPress(btnDown)) {
        if (brightnessLevel > 0) {
            brightnessLevel--;
            applyBrightness("PHIM PHU (GIAM)");
        } else {
            Serial.println("[CANH BAO] Do sang da o muc TAT (0/10)!");
        }
    }

    // CPU hoàn toàn rảnh rỗi (Non-blocking), không sử dụng delay()
}