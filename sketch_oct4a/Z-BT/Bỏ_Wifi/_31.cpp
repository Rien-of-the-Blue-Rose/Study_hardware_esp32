#include <Arduino.h>
#include <Firebase_ESP_Client.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "secrets.h"

// Đường dẫn Firebase riêng cho thiết bị
const String DEVICE_PATH = String("devices/") + USER_UID;
const String INT_PATH = DEVICE_PATH + "/test/int";

// Cấu hình nút nhấn GPIO14
const uint8_t BUTTON_PIN = 14;
const unsigned long DEBOUNCE_MS = 40;

// Thời gian chờ trước khi thử gửi lại nếu thất bại
const unsigned long UPLOAD_RETRY_MS = 1000;

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Biến đếm số lần nhấn nút
int count = 0;

// Các biến chống dội phím
bool lastRawButtonState = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// Các biến quản lý việc gửi Firebase
bool countNeedsUpload = false;
bool uploadRetryPending = false;
unsigned long lastUploadAttemptMillis = 0;

void setup() {
  Serial.begin(115200);

  // GPIO14 dùng điện trở kéo lên nội bộ
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Kết nối Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Cấu hình Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.printf("Button pin: GPIO%d\n", BUTTON_PIN);
  Serial.println("Ready to count button presses.");
}

void loop() {
  // 1. Đọc trạng thái hiện tại của nút
  bool reading = digitalRead(BUTTON_PIN);

  // 2. Phát hiện tín hiệu thay đổi
  if (reading != lastRawButtonState) {
    lastDebounceTime = millis();
    lastRawButtonState = reading;
  }

  // 3. Xác nhận trạng thái sau khi tín hiệu ổn định
  if ((millis() - lastDebounceTime >= DEBOUNCE_MS) &&
      (reading != stableButtonState)) {

    stableButtonState = reading;

    // INPUT_PULLUP: LOW nghĩa là nút đang được nhấn
    if (stableButtonState == LOW) {
      count++;
      countNeedsUpload = true;

      // Có số đếm mới, thử gửi ngay thay vì chờ lần thử lại cũ
      uploadRetryPending = false;

      Serial.printf("Button pressed. Count = %d\n", count);
    }
  }

  // 4. Gửi số đếm lên Firebase khi có thay đổi
  // Nếu thất bại, chờ 1 giây trước khi thử lại
  if (Firebase.ready() &&
      countNeedsUpload &&
      (!uploadRetryPending ||
       millis() - lastUploadAttemptMillis >= UPLOAD_RETRY_MS)) {

    lastUploadAttemptMillis = millis();

    if (Firebase.RTDB.setInt(&fbdo, INT_PATH, count)) {
      Serial.println("Firebase write PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
      Serial.printf("Uploaded count = %d\n", count);

      countNeedsUpload = false;
      uploadRetryPending = false;
    } else {
      Serial.println("Firebase write FAILED");
      Serial.println("REASON: " + fbdo.errorReason());

      // Giữ yêu cầu gửi để thử lại sau
      uploadRetryPending = true;
    }
  }
}

