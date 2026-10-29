// src/main.cpp
#include <Arduino.h>

// Danh sách chân GPIO kết nối với 8 LED theo thứ tự từ LED 0 (bit 0 - LSB) đến LED 7 (bit 7 - MSB)
const uint8_t LED_PINS[8] = {19, 18, 5, 17, 16, 4, 2, 15};

// Tổng số lượng LED trên thanh hiển thị
const uint8_t TOTAL_LEDS = 8;

// Xuất trạng thái 1 byte (8-bit) ra 8 chân LED tương ứng
// Mỗi bit của biến 'state' đại diện cho 1 LED (1: BẬT, 0: TẮT)
void setLedByte(uint8_t state) {
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        // Dịch bit thứ i về vị trí ngoài cùng bên phải, sau đó AND với 0x01 để lấy giá trị 0 hoặc 1
        digitalWrite(LED_PINS[i], (state >> i) & 0x01);
    }
}

// Biến lưu chỉ số hiệu ứng hiện tại (chạy tuần hoàn từ 0 đến 7)
uint8_t effect = 0;

// Biến đếm bước thực thi của hiệu ứng đang chạy
uint16_t step = 0;

// Mốc thời gian (ms) của lần cập nhật trạng thái LED gần nhất
unsigned long lastUpdate = 0;

// Chu kỳ thời gian (ms) giữa các bước; mỗi hiệu ứng có thể ghi đè giá trị này
unsigned long interval = 250;

void setup() {
    // Khởi tạo toàn bộ các chân điều khiển LED ở chế độ OUTPUT và kéo xuống mức LOW (tắt hết)
    for (uint8_t pin : LED_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
}

void loop() {
    unsigned long currentMillis = millis();

    // Guard Clause: Bộ điều phối thời gian bất đồng bộ thay thế cho delay()
    // Nếu chưa đủ thời gian interval trôi qua kể từ lần update trước thì thoát hàm ngay lập tức
    if (currentMillis - lastUpdate < interval) return;
    lastUpdate = currentMillis;

    // Cờ đánh dấu hiệu ứng hiện tại đã hoàn thành toàn bộ chu kỳ hay chưa
    bool effectDone = false;

    switch (effect) {
        // Hiệu ứng 1: Chớp/tắt đồng loạt toàn bộ 8 LED 3 lần
        case 0:
            interval = 250; // Chu kỳ chớp tắt: 250ms mỗi trạng thái
            // Bước chẵn (0, 2, 4): Bật sáng toàn bộ (0xFF = 11111111b)
            // Bước lẻ (1, 3, 5): Tắt toàn bộ (0x00 = 00000000b)
            setLedByte((step % 2 == 0) ? 0xFF : 0x00);
            step++;
            if (step >= 6) effectDone = true;
            break;

        // Hiệu ứng 2: Sáng dần rồi tắt dần theo hướng Trái -> Phải (LED 0 -> LED 7)
        case 1:
            interval = 100;
            if (step < 8) {
                // Giai đoạn 1 (bước 0..7): Sáng dần từ bit 0 lên bit 7
                // Phép tính (1 << (step + 1)) - 1 tạo ra mặt nạ liên tục gồm (step + 1) bit 1 tính từ LSB
                setLedByte((1 << (step + 1)) - 1);
            } else {
                // Giai đoạn 2 (bước 8..15): Tắt dần từ bit 0 lên bit 7
                // Lấy nghịch đảo (~) của mặt nạ các bit thấp để tắt dần các LED từ trái qua
                setLedByte(~((1 << (step - 7)) - 1));
            }
            step++;
            if (step >= 16) effectDone = true;
            break;

        // Hiệu ứng 3: Sáng dần rồi tắt dần theo hướng Phải -> Trái (LED 7 -> LED 0)
        case 2:
            interval = 100;
            if (step < 8) {
                // Giai đoạn 1 (bước 0..7): Sáng dồn từ bit MSB (bit 7) ngược về phía LSB
                // Dịch mặt nạ 0xFF sang trái (7 - step) bit để lấp đầy dần các bit cao
                setLedByte((0xFF << (7 - step)) & 0xFF);
            } else {
                // Giai đoạn 2 (bước 8..15): Tắt dần từ bit MSB ngược về phía LSB
                // Lấy nghịch đảo (~) của mặt nạ bit cao để hạ dần trạng thái các LED từ phải qua
                setLedByte(~((0xFF << (15 - step)) & 0xFF));
            }
            step++;
            if (step >= 16) effectDone = true;
            break;

        // Hiệu ứng 4: Quét qua lại 1 điểm sáng (Knight Rider) chạy 2 vòng lặp (14 bước/vòng, tổng 28 bước)
        case 3: {
            interval = 70;
            // Thu hẹp bước đếm về chu kỳ 14 nấc (quét đi 0..7, quét về 8..13)
            uint8_t sub = step % 14;
            // Ánh xạ quỹ đạo 2 chiều: đi từ 0 đến 7, rồi dội ngược về từ 6 xuống 1
            uint8_t pos = (sub <= 7) ? sub : (14 - sub);
            // Bật duy nhất 1 LED tại vị trí 'pos'
            setLedByte(1 << pos);
            step++;
            if (step >= 28) effectDone = true;
            break;
        }

        // Hiệu ứng 5: Sáng dồn từ Phải sang Trái (LED 7 chạy dồn về LED 0, tạo thành 8 khối tích lũy)
        // Tổng số bước chạy: 8 + 7 + 6 + 5 + 4 + 3 + 2 + 1 = 36 bước; bước 37 (index 36) dùng để dừng ngắm kết quả
        case 4: {
            if (step < 36) {
                interval = 60; // Tốc độ di chuyển của LED đang bay
                
                // Giải mã cấp số cộng để xác định:
                // - limit: số lượng LED đã về đích và đứng yên ở phía trái
                // - acc: tổng số bước tích lũy của các chu kỳ dồn trước đó
                uint8_t limit = 0, acc = 0;
                while (acc + (8 - limit) <= step) {
                    acc += (8 - limit);
                    limit++;
                }

                // pos: Vị trí của LED đang di chuyển (chạy lùi từ chân 7 về vị trí vạch 'limit')
                uint8_t pos = 7 - (step - acc);

                // fixedMask: Mặt nạ giữ sáng các LED đã chốt đích cố định bên trái (bit 0 đến bit limit - 1)
                uint8_t fixedMask = (limit > 0) ? ((1 << limit) - 1) : 0;

                // Gộp trạng thái LED đang chạy (1 << pos) với các LED đã cố định (fixedMask)
                setLedByte(fixedMask | (1 << pos));
            } else {
                interval = 300; // Dừng lại 300ms sau khi toàn bộ 8 LED đã sáng kín
            }
            step++;
            if (step > 36) effectDone = true;
            break;
        }

        // Hiệu ứng 6: Lan tỏa từ tâm (cặp LED giữa 3, 4) ra 2 biên ngoài, sau đó thu hẹp về lại tâm
        case 5:
            if (step < 4) {
                interval = 120;
                // Mẫu gốc 0x18 (00011000b) là 2 LED trung tâm. 
                // Dịch trái và dịch phải đồng thời để tạo hiệu ứng tách đôi dãn ra hai bên
                setLedByte((0x18 << step) | (0x18 >> step));
            } else if (step == 4) {
                interval = 100; // Nghỉ 100ms tại vị trí 2 biên ngoài cùng
            } else {
                interval = 120;
                uint8_t s = step - 5;
                // Thu hẹp dần từ 2 biên ngược về lại tâm bằng cách tắt dần các bit ngoài
                setLedByte(~(((1 << (s + 1)) - 1) | (((1 << (s + 1)) - 1) << (7 - s))));
            }
            step++;
            if (step >= 9) effectDone = true;
            break;

        // Hiệu ứng 7: Chớp tắt xen kẽ 4 chu kỳ (8 nấc chuyển trạng thái)
        case 6:
            interval = 200;
            // Bước chẵn: Bật LED vị trí chẵn (0x55 = 01010101b)
            // Bước lẻ: Bật LED vị trí lẻ (0xAA = 10101010b)
            setLedByte((step % 2 == 0) ? 0x55 : 0xAA);
            step++;
            if (step >= 8) effectDone = true;
            break;

        // Hiệu ứng 8: Đếm nhị phân hiển thị trực tiếp lên 8 LED (từ 0 đến 63)
        case 7:
            interval = 50; // Tăng số đếm mỗi 50ms
            setLedByte(step); // Xuất trực tiếp giá trị nhị phân của biến step ra LED
            step++;
            if (step >= 64) effectDone = true;
            break;
    }

    // Xử lý khi kết thúc một hiệu ứng:
    // Tắt toàn bộ LED, reset bước đếm về 0, chuyển tiếp sang hiệu ứng kế tiếp theo vòng tròn modulo 8
    if (effectDone) {
        setLedByte(0x00);
        step = 0;
        effect = (effect + 1) % 8;
    }
}
