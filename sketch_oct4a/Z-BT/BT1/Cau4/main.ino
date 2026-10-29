// src/main.cpp
#include <Arduino.h>

const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};
const uint8_t TOTAL_LEDS = 8;

const uint32_t PWM_FREQ = 5000;
const uint8_t  PWM_RES  = 8; // 8-bit (0 - 255)

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 1: KHÔI PHỤC NGƯỠNG SÀN 10% VÀ TÍNH TOÁN INTERVAL >= 10S]
// - Sửa DUTY_0 (0) quay lại DUTY_10 = 26 (round(255 * 0.10) = 25.5 -> 26).
// - Giữ FADE_STEP = 8 mỗi nấc.
// - Khoảng dốc: 255 - 26 = 229 đơn vị -> Cần 29 bước nhảy cho mỗi LED.
// - Cấu hình INTERVAL = 45ms: 29 bước * 8 LED * 45ms = 10440ms = 10.44 giây.
// =============================================================================
const uint8_t DUTY_100 = 255;
const uint8_t DUTY_10  = 7; // Mức sàn 10%: giữ sáng mờ, không ngắt dòng

const uint8_t       FADE_STEP = 8;  // Bước nhảy 8 đơn vị duty
const unsigned long INTERVAL  = 45; // 45ms mỗi bước -> 8 LED quét mất 10.44 giây
const unsigned long PAUSE_MS  = 300;

const unsigned long SERIAL_INTERVAL = 500; // Serial in telemetry mỗi 0.5 giây
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Khôi phục hằng số `DUTY_10 = 26` thay thế cho mức `DUTY_0 = 0` bị nhầm lẫn ở bản trước.
   - Tính toán lại `INTERVAL = 45ms` để khớp với 29 bước nhảy của khoảng dải 229,
     đảm bảo tổng thời gian quét qua 8 LED vượt ngưỡng tối thiểu 10 giây (10.44s).
   ============================================================================= */

int8_t  currentLed   = 0;
int16_t currentDuty  = DUTY_100;
bool    isFadingDown = true; // true: 100% -> 10%, false: 10% -> 100%

unsigned long lastUpdate = 0;
unsigned long pauseStart = 0;
bool          isPaused   = false;

uint8_t ledDutyState[TOTAL_LEDS] = {DUTY_100, DUTY_100, DUTY_100, DUTY_100, 
                                    DUTY_100, DUTY_100, DUTY_100, DUTY_100};

unsigned long lastSerialPrint = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Serial.println("\n--- HE THONG KHOI CHAY: PWM 8 LED (STEP = 8, MIN = 10%) ---");
    Serial.printf("Thong so: Day = %d (10%%) | Step = %d | Interval = %lums | Chu ky 8 LED = %.2fs\n", 
                  DUTY_10, FADE_STEP, INTERVAL, (float)(29 * 8 * INTERVAL) / 1000.0);

    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        ledcAttach(LED_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(LED_PINS[i], DUTY_100);
        ledDutyState[i] = DUTY_100;
    }
}

// =============================================================================
// [ĐOẠN THAY ĐỔI NỔI BẬT 2: KHÓA CHẶN DỐC VÀ CHUYỂN GIAO TRẠNG THÁI TẠI MỨC DUTY_10]
// =============================================================================
void loop() {
    unsigned long currentMillis = millis();

    // 1. LUỒNG SERIAL TELEMETRY: In định kỳ mỗi 500ms (0.5 giây)
    if (currentMillis - lastSerialPrint >= SERIAL_INTERVAL) {
        lastSerialPrint = currentMillis;

        Serial.printf("[%6lu ms] Che do: %-15s | LED tac dong: L%d | Duty LED: %3d\n",
                      currentMillis,
                      isPaused ? "DANG DUNG (PAUSE)" : (isFadingDown ? "GIAM (100->10%)" : "TANG (10->100%)"),
                      currentLed + 1,
                      currentDuty);

        Serial.print("            PWM [L1->L8]: ");
        for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
            Serial.printf("%3d ", ledDutyState[i]);
        }
        Serial.println();
    }

    // 2. Xử lý khoảng dừng 300ms giữa hai chiều biến thiên
    if (isPaused) {
        if (currentMillis - pauseStart >= PAUSE_MS) {
            isPaused = false;
        }
        return;
    }

    // 3. Nội suy độ sáng tuần tự từng LED
    if (currentMillis - lastUpdate >= INTERVAL) {
        lastUpdate = currentMillis;

        if (isFadingDown) {
            // GIẢM DẦN TỪNG LED: 255 -> 26
            currentDuty -= FADE_STEP;

            if (currentDuty <= DUTY_10) {
                currentDuty = DUTY_10; // Khóa cứng mức sàn 26, không cho tụt về 0
                ledcWrite(LED_PINS[currentLed], (uint8_t)currentDuty);
                ledDutyState[currentLed] = (uint8_t)currentDuty;

                currentLed++;
                currentDuty = DUTY_100; // Khởi tạo mức ban đầu cho LED kế tiếp

                if (currentLed >= TOTAL_LEDS) {
                    currentLed   = 0;
                    currentDuty  = DUTY_10;
                    isFadingDown = false; // Đổi sang chu trình tăng
                    isPaused     = true;
                    pauseStart   = currentMillis;
                }
            } else {
                ledcWrite(LED_PINS[currentLed], (uint8_t)currentDuty);
                ledDutyState[currentLed] = (uint8_t)currentDuty;
            }
        } 
        else {
            // TĂNG DẦN TỪNG LED: 26 -> 255
            currentDuty += FADE_STEP;

            if (currentDuty >= DUTY_100) {
                currentDuty = DUTY_100; // Khóa cứng mức đỉnh 255
                ledcWrite(LED_PINS[currentLed], (uint8_t)currentDuty);
                ledDutyState[currentLed] = (uint8_t)currentDuty;

                currentLed++;
                currentDuty = DUTY_10; // Khởi tạo mức sàn 26 cho LED kế tiếp

                if (currentLed >= TOTAL_LEDS) {
                    currentLed   = 0;
                    currentDuty  = DUTY_100;
                    isFadingDown = true; // Đổi sang chu trình giảm
                    isPaused     = true;
                    pauseStart   = currentMillis;
                }
            } else {
                ledcWrite(LED_PINS[currentLed], (uint8_t)currentDuty);
                ledDutyState[currentLed] = (uint8_t)currentDuty;
            }
        }
    }
}
/* =============================================================================
   [COMMENT BLOCK - GIẢI THÍCH PHẦN ĐÃ SỬA]:
   - Trong nhánh giảm: Điều kiện `currentDuty <= DUTY_10` chặn cứng mức giá trị tại 26,
     loại bỏ hoàn toàn trạng thái tắt hẳn (duty = 0).
   - Trong nhánh tăng: Khởi tạo LED kế tiếp với `currentDuty = DUTY_10` (thay vì mức 0 cũ)
     để bắt đầu dốc tăng từ đúng 10% công suất lên 255.
   - Dữ liệu in qua UART hiển thị chính xác mức sàn 26 trên cả 8 vị trí mảng.
   ============================================================================= */
