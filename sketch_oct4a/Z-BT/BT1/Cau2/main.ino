// src/main.cpp
#include <Arduino.h>

const uint8_t BUTTON_PIN = 0;               // Chân GPIO 0 (Nút BOOT trên ESP32)
const unsigned long DEBOUNCE_TIME_MS = 50;  // Thời gian lọc nhiễu tiếp điểm (50ms)

uint32_t pressCount = 0;                    // Biến đếm tích lũy (tăng liên tục qua từng lần nhấn)

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT: THAY THẾ TOÀN BỘ LOGIC CŨ BẰNG MÁY TRẠNG THÁI 4 PHA (FSM)]
// - Khắc phục triệt để nguy cơ kẹt cờ: Phân rã quá trình nhấn thành chu trình khép kín:
//   Chờ nhấn -> Lọc nhiễu nhấn -> Đếm & Chờ nhả -> Lọc nhiễu nhả -> Quay lại Chờ nhấn.
// - Đảm bảo chu kỳ: Bấm lần 1 (+1) -> Nhả -> Bấm lần 2 (+1) -> ... -> Bấm lần N (+1).
// =============================================================================

enum ButtonState {
    BTN_IDLE,              // Pha 0: Nút đang thả, chờ người dùng nhấn
    BTN_DEBOUNCE_PRESS,    // Pha 1: Phát hiện mức LOW, đếm thời gian xác thực nhấn thật
    BTN_HELD,              // Pha 2: Đã cộng đếm, giữ trạng thái khóa (chống cộng dồn khi đè nút)
    BTN_DEBOUNCE_RELEASE   // Pha 3: Phát hiện mức HIGH, đếm thời gian xác thực nhả thật
};

ButtonState currentBtnState = BTN_IDLE;
unsigned long stateTimer = 0;

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Kích hoạt trở kéo lên nội bộ (Mặc định nhả = HIGH)
    delay(200);
    Serial.println("==========================================");
    Serial.println("   ESP32 CONTINUOUS BUTTON COUNTER FSM    ");
    Serial.println("==========================================");
    Serial.println("San sang: Moi lan nhan-nha se tang dung 1 don vi.");
}

void loop() {
    int pinValue = digitalRead(BUTTON_PIN);
    unsigned long currentMillis = millis();

    switch (currentBtnState) {
        // Pha 0: Chờ nhấn nút
        case BTN_IDLE:
            if (pinValue == LOW) { // Bắt đầu chạm phím
                stateTimer = currentMillis;
                currentBtnState = BTN_DEBOUNCE_PRESS;
            }
            break;

        // Pha 1: Lọc nhiễu sườn xuống
        case BTN_DEBOUNCE_PRESS:
            if (pinValue == HIGH) { 
                // Nhiễu nảy tiếp điểm cơ khí nhả ra ngay < 50ms -> Bỏ qua, về IDLE
                currentBtnState = BTN_IDLE;
            } else if (currentMillis - stateTimer >= DEBOUNCE_TIME_MS) {
                // Đã nhấn giữ ổn định đủ 50ms -> XÁC NHẬN MỘT LẦN NHẤN HỢP LỆ
                pressCount++; // Tăng đúng 1 đơn vị cho lần nhấn này
                Serial.printf("[NHAN LAN %u] Gia tri dem hien tai: %u\n", pressCount, pressCount);
                currentBtnState = BTN_HELD;
            }
            break;

        // Pha 2: Chờ người dùng nhả nút (Dù giữ nút 10 giây cũng không bao giờ tăng thêm)
        case BTN_HELD:
            if (pinValue == HIGH) { // Bắt đầu nhả tay khỏi nút
                stateTimer = currentMillis;
                currentBtnState = BTN_DEBOUNCE_RELEASE;
            }
            break;

        // Pha 3: Lọc nhiễu sườn lên khi nhả
        case BTN_DEBOUNCE_RELEASE:
            if (pinValue == LOW) {
                // Tiếp điểm vẫn nảy về LOW -> Cập nhật lại mốc chờ
                stateTimer = currentMillis;
            } else if (currentMillis - stateTimer >= DEBOUNCE_TIME_MS) {
                // Đã nhả tay ổn định đủ 50ms -> TÁI LẬP TRẠNG THÁI BAN ĐẦU
                currentBtnState = BTN_IDLE; // Sẵn sàng 100% cho lần bấm kế tiếp
            }
            break;
    }
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH CHI TIẾT ĐOẠN ĐÃ SỬA]:
   1. Cơ chế lặp vô hạn (Continuous Trigger): Khác với mã cũ chỉ so sánh 2 mốc
      `lastRawReading`, FSM chia rõ việc "nhấn" và "nhả" thành các pha riêng biệt.
   2. Khi nhả nút thành công qua pha `BTN_DEBOUNCE_RELEASE`, FSM tự động reset 
      về `BTN_IDLE`. Điều này đảm bảo lần bấm thứ 2, thứ 3... thứ N đều được hệ
      thống tiếp nhận bình thường.
   3. Khóa giữ mức (Interlock): Khi đang ở `BTN_HELD`, biến `pressCount` tuyệt 
      đối không bị cộng dồn liên tục nếu người dùng ấn giữ đè nút.
   ============================================================================= */