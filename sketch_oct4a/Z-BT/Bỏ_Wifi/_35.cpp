// path: _30.cpp

/* ==========================================================================
   SECTION 1: Nạp các thư viện cần thiết
   ========================================================================== */
#include <Arduino.h>
#include <Firebase_ESP_Client.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

/* ==========================================================================
   SECTION 2: Nạp các helper addons
   ========================================================================== */
// Cung cấp thông tin về quá trình tạo token.
#include "addons/TokenHelper.h"

// Cung cấp các hàm hỗ trợ Firebase Realtime Database.
#include "addons/RTDBHelper.h"

#include "secrets.h"

/* ==========================================================================
   SECTION 3: Khai báo thông tin mạng Wi-Fi
   ========================================================================== */
// WIFI_SSID và WIFI_PASSWORD được khai báo trong secrets.h.

/* ==========================================================================
   SECTION 4: Khai báo Firebase API Key
   ========================================================================== */
// API_KEY được khai báo trong secrets.h.

/* ==========================================================================
   SECTION 5: Khai báo Firebase Realtime Database URL
   ========================================================================== */
// DATABASE_URL được khai báo trong secrets.h.

/* ==========================================================================
   SECTION 6: Khởi tạo Firebase và các biến điều khiển
   ========================================================================== */
// Các đối tượng Firebase.
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Các biến quản lý chu kỳ đọc dữ liệu.
unsigned long sendDataPrevMillis = 0;
const unsigned long READ_INTERVAL = 15000;

// Trạng thái xác thực.
bool signupOK = false;

// Các biến lưu dữ liệu đọc từ Firebase.
int intValue;
float floatValue;

// GPIO điều khiển LED và nút nhấn.
const int LED_PIN = 2;
const int BUTTON_PIN = 14;

// Trạng thái LED.
bool ledState = false;

// Các biến chống rung nút nhấn (debounce).
const unsigned long DEBOUNCE_DELAY = 50;

unsigned long lastDebounceTime = 0;

int lastButtonReading = HIGH;
int stableButtonState = HIGH;

// Quản lý việc ghi trạng thái LED lên Firebase.
bool ledStatePendingWrite = false;

const unsigned long LED_WRITE_RETRY_INTERVAL = 1000;
unsigned long lastLedWriteAttempt = 0;

/* ==========================================================================
   SECTION 7: Khởi tạo Serial, GPIO và kết nối Wi-Fi
   ========================================================================== */
void setup() {
  Serial.begin(115200);

  // Khởi tạo GPIO2 điều khiển LED.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Khởi tạo GPIO14 làm nút nhấn với điện trở kéo lên nội.
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("===== ESP32 Firebase LED Controller =====");

  // Kết nối Wi-Fi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* ========================================================================
     SECTION 8: Cấu hình Firebase API Key và RTDB URL
     ======================================================================== */
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  /* ========================================================================
     SECTION 9: Cấu hình xác thực Email/Password
     ======================================================================== */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Cho phép chương trình xử lý khi Firebase sẵn sàng.
  // Firebase.begin() sẽ thực hiện quá trình xác thực.
  signupOK = true;

  /* ========================================================================
     SECTION 10: Callback Token và khởi động Firebase
     ======================================================================== */
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);

  // Tự động kết nối lại Wi-Fi khi cần.
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase initialization completed.");
  Serial.printf("LED GPIO: %d\n", LED_PIN);
  Serial.printf("Button GPIO: %d\n", BUTTON_PIN);
  Serial.println("=========================================");
}

/* ==========================================================================
   SECTION 11: Đọc nút nhấn và toggle trạng thái LED
   ========================================================================== */
void loop() {
  // Đọc trạng thái hiện tại của nút nhấn.
  int buttonReading = digitalRead(BUTTON_PIN);

  // Phát hiện thay đổi tín hiệu để bắt đầu chống rung.
  if (buttonReading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  // Chấp nhận thay đổi khi tín hiệu đã ổn định đủ lâu.
  if (millis() - lastDebounceTime >= DEBOUNCE_DELAY) {

    // Chỉ xử lý khi trạng thái ổn định thực sự thay đổi.
    if (buttonReading != stableButtonState) {
      stableButtonState = buttonReading;

      // INPUT_PULLUP: LOW nghĩa là nút được nhấn.
      if (stableButtonState == LOW) {

        /* ------------------------------------------------------------------
           FIX: Toggle trạng thái LED mỗi lần nhấn nút.
           ------------------------------------------------------------------ */
        ledState = !ledState;

        // Điều khiển LED cục bộ ngay lập tức.
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);

        // Đánh dấu trạng thái mới cần ghi lên Firebase.
        ledStatePendingWrite = true;

        Serial.printf(
          "[BUTTON] GPIO%d pressed | ledState = %s\n",
          BUTTON_PIN,
          ledState ? "true" : "false"
        );

        Serial.printf(
          "[LED] GPIO%d = %s\n",
          LED_PIN,
          ledState ? "ON" : "OFF"
        );
      }
    }
  }

  // Lưu giá trị hiện tại để phát hiện thay đổi ở vòng lặp tiếp theo.
  lastButtonReading = buttonReading;

  /* ==========================================================================
     SECTION 12: Đồng bộ trạng thái LED lên Firebase
     ========================================================================== */
  if (
    Firebase.ready() &&
    signupOK &&
    ledStatePendingWrite &&
    (
      millis() - lastLedWriteAttempt >= LED_WRITE_RETRY_INTERVAL ||
      lastLedWriteAttempt == 0
    )
  ) {
    lastLedWriteAttempt = millis();

    // Ghi giá trị Boolean lên /test/ledState.
    if (Firebase.RTDB.setBool(
          &fbdo,
          "/test/ledState",
          ledState
        )) {

      Serial.printf(
        "[RTDB] Write test/ledState = %s SUCCESS\n",
        ledState ? "true" : "false"
      );

      // Xóa cờ sau khi ghi thành công.
      ledStatePendingWrite = false;

      // Bắt đầu lại chu kỳ đọc dữ liệu sau khi ghi.
      sendDataPrevMillis = millis();

    } else {

      Serial.printf(
        "[RTDB] Write test/ledState FAILED: %s\n",
        fbdo.errorReason().c_str()
      );

      // Giữ cờ pending để tự động thử ghi lại ở vòng lặp sau.
    }
  }

  /* ==========================================================================
     SECTION 13: Đọc dữ liệu định kỳ từ Firebase mỗi 15 giây
     ========================================================================== */
  if (
    Firebase.ready() &&
    signupOK &&
    !ledStatePendingWrite &&
    (
      millis() - sendDataPrevMillis >= READ_INTERVAL ||
      sendDataPrevMillis == 0
    )
  ) {
    sendDataPrevMillis = millis();

    /* ========================================================================
       SECTION 14: Đọc số nguyên từ test/int
       ======================================================================== */
    if (Firebase.RTDB.getInt(&fbdo, "/test/int")) {

      if (fbdo.dataType() == "int") {
        intValue = fbdo.intData();

        Serial.printf(
          "[RTDB] test/int = %d\n",
          intValue
        );
      } else {
        Serial.println("[RTDB] Unexpected data type at test/int");
      }

    } else {
      Serial.printf(
        "[RTDB] Read test/int FAILED: %s\n",
        fbdo.errorReason().c_str()
      );
    }

    /* ========================================================================
       SECTION 15: Đọc số thực từ test/float
       ======================================================================== */
    if (Firebase.RTDB.getFloat(&fbdo, "/test/float")) {

      if (fbdo.dataType() == "float") {
        floatValue = fbdo.floatData();

        Serial.printf(
          "[RTDB] test/float = %.2f\n",
          floatValue
        );
      } else {
        Serial.println("[RTDB] Unexpected data type at test/float");
      }

    } else {
      Serial.printf(
        "[RTDB] Read test/float FAILED: %s\n",
        fbdo.errorReason().c_str()
      );
    }

    /* ========================================================================
       SECTION 16: Đọc trạng thái LED từ test/ledState
       ======================================================================== */
    if (Firebase.RTDB.getBool(&fbdo, "/test/ledState")) {

      if (fbdo.dataType() == "boolean") {

        // Cập nhật trạng thái LED theo Firebase.
        ledState = fbdo.boolData();

        digitalWrite(
          LED_PIN,
          ledState ? HIGH : LOW
        );

        Serial.printf(
          "[RTDB] test/ledState = %s\n",
          ledState ? "true" : "false"
        );

        Serial.printf(
          "[LED] GPIO%d = %s\n",
          LED_PIN,
          ledState ? "ON" : "OFF"
        );

      } else {
        Serial.println(
          "[RTDB] Unexpected data type at test/ledState"
        );
      }

    } else {
      Serial.printf(
        "[RTDB] Read test/ledState FAILED: %s\n",
        fbdo.errorReason().c_str()
      );
    }
  }
}