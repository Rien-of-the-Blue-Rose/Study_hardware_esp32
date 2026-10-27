
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
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "secrets.h"

/* ==========================================================================
   SECTION 3: Cấu hình điều khiển LED bằng PWM
   ========================================================================== */
const uint8_t LED_PIN = 2;
const uint8_t PWM_RESOLUTION_BITS = 6;
const uint32_t PWM_FREQUENCY = 5000;

// Firebase được đọc lại sau mỗi 1000 ms.
const unsigned long READ_INTERVAL_MS = 1000;

/* ==========================================================================
   SECTION 4: Khởi tạo Firebase và các biến điều khiển
   ========================================================================== */
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastReadMillis = 0;

int intValue = 0;
int brightness = 0;

bool signupOK = false;

void setup() {
  /* ========================================================================
     SECTION 5: Khởi tạo Serial và PWM cho LED GPIO2
     ======================================================================== */
  Serial.begin(115200);

  analogWriteResolution(LED_PIN, PWM_RESOLUTION_BITS);
  analogWriteFrequency(LED_PIN, PWM_FREQUENCY);
  analogWrite(LED_PIN, 0);

  /* ------------------------------------------------------------------------
     HIỆU CHỈNH SECTION 5:
     - GPIO2 xuất tín hiệu PWM để điều khiển độ sáng LED.
     - Độ phân giải 6 bit: giá trị PWM từ 0 đến 63.
     - Tần số PWM: 5000 Hz.
     - Giá trị ban đầu bằng 0 để LED tắt.
     ------------------------------------------------------------------------ */

  /* ========================================================================
     SECTION 6: Kết nối Wi-Fi
     ======================================================================== */
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* ========================================================================
     SECTION 7: Cấu hình Firebase
     ======================================================================== */
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  /* ========================================================================
     SECTION 8: Xác thực tài khoản
     ======================================================================== */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  signupOK = true;

  /* ========================================================================
     SECTION 9: Callback và khởi động Firebase
     ======================================================================== */
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  /* ========================================================================
     SECTION 10: Kiểm tra trạng thái Firebase và chu kỳ đọc dữ liệu
     ======================================================================== */
  if (!Firebase.ready() || !signupOK) {
    return;
  }

  if (lastReadMillis != 0 &&
      millis() - lastReadMillis < READ_INTERVAL_MS) {
    return;
  }

  lastReadMillis = millis();

  /* ========================================================================
     SECTION 11: Đọc int từ /test/int và điều khiển độ sáng LED
     ======================================================================== */
  if (Firebase.RTDB.getInt(&fbdo, "/test/int")) {

    if (fbdo.dataType() == "int") {
      // Đọc giá trị int từ Firebase.
      intValue = fbdo.intData();

      // Giới hạn độ sáng trong khoảng [0, 63].
      brightness = constrain(intValue, 0, 63);

      // Cập nhật độ sáng LED GPIO2 bằng PWM 6 bit.
      analogWrite(LED_PIN, brightness);

      Serial.printf(
        "Firebase int = %d | LED GPIO%d brightness = %d/63\n",
        intValue,
        LED_PIN,
        brightness
      );

      /* --------------------------------------------------------------------
         HIỆU CHỈNH SECTION 11:
         - Đọc số nguyên tại /test/int.
         - constrain() giới hạn giá trị về 0..63.
         - analogWrite() áp dụng giá trị PWM lên GPIO2.
         - Chỉ cập nhật LED khi đọc được dữ liệu kiểu int hợp lệ.
         -------------------------------------------------------------------- */

    } else {
      Serial.printf(
        "Unexpected data type at /test/int: %s\n",
        fbdo.dataType().c_str()
      );
    }

  } else {
    Serial.printf(
      "Firebase getInt failed: %s\n",
      fbdo.errorReason().c_str()
    );
  }
}
