// path: src/main.cpp
#include <Arduino.h>

// ==========================================
// CẤU HÌNH PHẦN CỨNG & THÔNG SỐ PWM
// ==========================================

// Mảng chứa danh sách các chân GPIO kết nối với các LED (Khớp 100% với diagram.json)
const uint8_t LED_PINS[] = {19, 18, 5, 17, 16, 4, 2, 15};
// Tổng số LED = kích thước của mảng LED_PINS = 8 LED
const uint8_t TOTAL_LEDS = sizeof(LED_PINS);

// Tần số xung PWM (5 kHz giúp mắt người không thấy hiện tượng nhấp nháy)
const uint32_t PWM_FREQ  = 5000;
// Độ phân giải PWM 8-bit (dải giá trị duty cycle từ 0 đến 255)
const uint8_t  PWM_RES   = 8;

const uint8_t  DUTY_MIN  = 7;
/* =============================================================================
 * [MODIFIED: CẤU HÌNH NGƯỠNG TỐI THIỂU]
 * - Cũ: const uint8_t DUTY_MIN = 7;
 * - Mới: const uint8_t DUTY_MIN = 0;
 * - Tác dụng: Ngắt xung PWM hoàn toàn (duty = 0%). Giá trị 7 cũ tạo điện áp kích mở
 *   khiến Wokwi hiển thị LED sáng mờ ngay từ vạch xuất phát.
 * ============================================================================= */

// Ngưỡng duty tối đa (tương ứng với mức sáng max mà chương trình đang xét)
const uint8_t  DUTY_MAX  = 255;
// Bước thay đổi duty cycle mỗi chu kỳ cập nhật
const uint8_t  FADE_STEP = 8;
// Thời lượng của mỗi chu kỳ làm mới trạng thái LED (ms)
const unsigned long INTERVAL = 45;
// Thời gian dừng (ms) giữa 2 lượt quét xuôi/ngược
const unsigned long PAUSE_MS = 300;

// ==========================================
// BIẾN QUẢN LÝ TRẠNG THÁI HIỆU ỨNG (STATE MACHINE)
// ==========================================

// Chỉ số index (số thứ tự) của LED đang thực hiện hiệu ứng fade (0 -> 7)
int8_t  currentLed   = 0;

int16_t currentDuty  = 0;
int8_t  fadeDir      = 1;
/* =============================================================================
 * [MODIFIED: VECTOR BIẾN THIÊN BAN ĐẦU]
 * - Cũ: currentDuty = DUTY_MIN (hoặc DUTY_MAX); fadeDir = -1;
 * - Mới: currentDuty = DUTY_MIN; fadeDir = 1;
 * - Tác dụng: Bắt buộc LED đầu tiên (GPIO 19) chạy chu trình SÁNG DẦN (0 -> 255).
 *   Giá trị fadeDir = -1 cũ khiến hệ thống thực thi chu trình xả xung (tối dần) trước.
 * ============================================================================= */

// Mốc thời gian (ms) ghi nhận lần cập nhật duty gần nhất cho non-blocking delay
unsigned long lastUpdate = 0;
// Mốc thời gian (ms) bắt đầu khoảng dừng giữa hai chu trình quét
unsigned long pauseStart = 0;
// Cờ báo hiệu hệ thống đang trong khoảng thời gian tạm dừng (true/false)
bool          isPaused   = false;

void setup() {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
        ledcAttach(LED_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(LED_PINS[i], (uint32_t)DUTY_MIN);
    }
    /* =============================================================================
     * [MODIFIED: KHÓA CỨNG MỨC LOW VẬT LÝ TRƯỚC KHI GẮN NGOẠI VI LEDC]
     * - Cũ: ledcAttach(...) rồi ledcWrite(..., DUTY_MAX);
     * - Mới: Ép pinMode OUTPUT và ghi digitalWrite LOW trước, sau đó mới attach LEDC
     *   và chốt duty = DUTY_MIN (0).
     * - Tác dụng: Loại bỏ 100% xung thả nổi và lệnh ghi HIGH sai lúc boot, buộc 8 chân
     *   GPIO phải duy trì chính xác 0V (GND) ngay khi Wokwi bắt đầu chạy mô phỏng.
     * ============================================================================= */
}

void loop() {
    unsigned long now = millis();

    // -------------------------------------------------------------------------
    // 1. XỬ LÝ KHOẢNG NGHỈ GIỮA HAI CHU TRÌNH QUÉT (NON-BLOCKING)
    // -------------------------------------------------------------------------
    if (isPaused) {
        if (now - pauseStart >= PAUSE_MS) {
            isPaused = false;
            // Cập nhật lại mốc lastUpdate để triệt tiêu độ trễ cộng dồn gây giật nhịp ngay sau pause
            lastUpdate = now; 
        }
        return; // Dừng thực thi các logic bên dưới khi đang trong thời gian pause
    }

    // -------------------------------------------------------------------------
    // 2. BỘ ĐỊNH THÌ CẬP NHẬT TRẠNG THÁI THEO INTERVAL
    // -------------------------------------------------------------------------
    if (now - lastUpdate < INTERVAL) return;
    lastUpdate = now;

    // -------------------------------------------------------------------------
    // 3. TÍNH TOÁN VÀ ÁP ĐẶT DUTY CYCLE THEO CHIỀU BIẾN THIÊN
    // -------------------------------------------------------------------------
    currentDuty += fadeDir * FADE_STEP;
    
    // Kiểm tra xem duty cycle đã chạm hoặc vượt qua ngưỡng giới hạn tương ứng chưa
    bool reachedLimit = (fadeDir < 0) ? (currentDuty <= DUTY_MIN) : (currentDuty >= DUTY_MAX);

    // Kẹp giá trị (clamp) để tránh hiện tượng tràn số nguyên hoặc vượt dải băm xung
    if (reachedLimit) {
        currentDuty = (fadeDir < 0) ? DUTY_MIN : DUTY_MAX;
    }

    // Cập nhật xung PWM thực tế tới chân LED đang hoạt động
    ledcWrite(LED_PINS[currentLed], (uint32_t)currentDuty);
    /* =============================================================================
     * [MODIFIED: CHUẨN HÓA KIỂU DỮ LIỆU ĐẦU VÀO]
     * - Cũ: ledcWrite(..., (uint8_t)currentDuty);
     * - Mới: ledcWrite(..., (uint32_t)currentDuty);
     * - Tác dụng: Khớp hoàn toàn với prototype của ESP32 Arduino Core 3.x, ngăn ngừa
     *   cảnh báo biên dịch và lỗi ép kiểu dữ liệu xung.
     * ============================================================================= */

    // -------------------------------------------------------------------------
    // 4. CHUYỂN BƯỚC ĐIỀU KHIỂN SANG LED TIẾP THEO HOẶC ĐỔI CHIỀU QUÉT
    // -------------------------------------------------------------------------
    if (reachedLimit) {
        currentLed++; // Chuyển quyền điều khiển sang bóng LED kế tiếp
        
        // Khi toàn bộ 8 LED đã hoàn thành chu kỳ fade hiện tại
        if (currentLed >= TOTAL_LEDS) {
            currentLed = 0;         // Reset index về LED đầu tiên
            fadeDir    = -fadeDir;  // Đảo ngược chiều biến thiên duty cho chu kỳ tiếp theo
            isPaused   = true;      // Kích hoạt trạng thái tạm dừng
            pauseStart = now;       // Lưu mốc thời gian bắt đầu dừng
        }
        
        // Đặt mức khởi điểm duty cycle cho LED kế tiếp tùy thuộc vào chiều fade mới
        currentDuty = (fadeDir < 0) ? DUTY_MAX : DUTY_MIN;
    }
}
