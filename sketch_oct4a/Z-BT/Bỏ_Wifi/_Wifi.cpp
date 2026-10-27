
#include <WiFi.h>

// Replace with your network credentials (STATION)
#define ssid "Wokwi-GUEST"
#define password ""

const int LED_STATUS_PIN = 15; // D15: LED trạng thái mạng
const int LED_WEAK_PIN   = 2;  // D2:  LED mức 1 (Sóng yếu)
const int LED_MED_PIN    = 4;  // D4:  LED mức 2 (Sóng trung bình)
const int LED_STRONG_PIN = 16; // D16: LED mức 3 (Sóng mạnh)

void tatToanBoLedSong();
void xuatTrangThaiHeThong(bool daKetNoi, long rssi);
void inTrangThaiGPIO(uint8_t status, uint8_t weak, uint8_t med, uint8_t strong);

void setup() {
  Serial.begin(115200);

  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(LED_WEAK_PIN, OUTPUT);
  pinMode(LED_MED_PIN, OUTPUT);
  pinMode(LED_STRONG_PIN, OUTPUT);

  digitalWrite(LED_STATUS_PIN, LOW);
  tatToanBoLedSong();

  Serial.println("\n--- Start ---");
  WiFi.begin(ssid, password);

  // Nhấp nháy LED STATUS khi đang dò mạng, 3 LED sóng tắt hoàn toàn
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_STATUS_PIN, !digitalRead(LED_STATUS_PIN));
    delay(500);
    Serial.print(".");
  }

  // Kết nối thành công
  digitalWrite(LED_STATUS_PIN, HIGH);
  Serial.println("\n[WIFI] Ket noi thanh cong!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    long rssi = WiFi.RSSI();
    Serial.printf("[ONLINE] RSSI: %ld dBm\n", rssi);

    // Xuất đồng thời 1 LED trạng thái và 3 LED mức sóng
    xuatTrangThaiHeThong(true, rssi);
  } else {
    Serial.println("[OFFLINE] Mat ket noi! Dang ket noi lai...");
    
    // Tắt toàn bộ 4 LED khi mất mạng
    xuatTrangThaiHeThong(false, 0);

    WiFi.disconnect();
    WiFi.reconnect();
  }

  delay(2000);
}

// Tắt toàn bộ 3 LED cột sóng
void tatToanBoLedSong() {
  digitalWrite(LED_WEAK_PIN, LOW);
  digitalWrite(LED_MED_PIN, LOW);
  digitalWrite(LED_STRONG_PIN, LOW);
}

void inTrangThaiGPIO(uint8_t status, uint8_t weak, uint8_t med, uint8_t strong) {
  Serial.printf("  [GPIO OUTPUT] D15(STATUS): %d | D2(WEAK): %d | D4(MED): %d | D16(STRONG): %d\n",
                status, weak, med, strong);
}

void xuatTrangThaiHeThong(bool daKetNoi, long rssi) {
  static const char* const NHAN_COT_SONG[] = {
    "KHONG DUNG DUOC (0/3 - Signal degraded)", // Level 0: < -90 dBm
    "YEU (1/3)",                              // Level 1: -90 đến -71 dBm
    "TRUNG BINH (2/3)",                       // Level 2: -70 đến -51 dBm
    "MANH (3/3)"                              // Level 3: >= -50 dBm
  };

  uint8_t status = daKetNoi ? HIGH : LOW;
  uint8_t level  = 0;

  if (daKetNoi) {
    if (rssi >= -50)      level = 3;
    else if (rssi >= -70) level = 2;
    else if (rssi >= -90) level = 1;
    else                  level = 0;
  }

  // Tự động phân giải logic đèn dựa trên cấp độ sóng
  uint8_t weak   = (level >= 1) ? HIGH : LOW;
  uint8_t med    = (level >= 2) ? HIGH : LOW;
  uint8_t strong = (level >= 3) ? HIGH : LOW;

  // Ghi phần cứng tập trung (DRY) - không phân mảnh rải rác
  digitalWrite(LED_STATUS_PIN, status);
  digitalWrite(LED_WEAK_PIN,   weak);
  digitalWrite(LED_MED_PIN,    med);
  digitalWrite(LED_STRONG_PIN, strong);

  // Xuất Serial tập trung
  if (daKetNoi) {
    Serial.printf("-> Cột sóng: %s\n", NHAN_COT_SONG[level]);
  }
  inTrangThaiGPIO(status, weak, med, strong);
}
