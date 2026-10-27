
// path: _30.cpp

/* ==========================================================================
   SECTION 1: Nạp thư viện
   ========================================================================== */
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <WiFi.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "secrets.h"

/* ==========================================================================
   SECTION 2: Khởi tạo Firebase
   ========================================================================== */
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

/* ==========================================================================
   SECTION 3: Khai báo GPIO và biến điều khiển
   ========================================================================== */
const int LDR_PIN = 39;
const int LED_PIN = 2;

const float INITIAL_THRESHOLD = 1.5f;
float floatValue = INITIAL_THRESHOLD;
float sensorVoltage = 0.0f;
int sensorRaw = 0;

bool thresholdInitialized = false;

unsigned long lastFirebaseRead = 0;
const unsigned long FIREBASE_INTERVAL = 15000;

/* ==========================================================================
   SECTION 4: Khởi tạo hệ thống
   ========================================================================== */
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  /* ------------------------------------------------------------------------
     SECTION 4.1: Kết nối Wi-Fi
     ------------------------------------------------------------------------ */
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());

  /* ------------------------------------------------------------------------
     SECTION 4.2: Cấu hình Firebase
     ------------------------------------------------------------------------ */
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  /* ------------------------------------------------------------------------
     SECTION 4.3: Khởi tạo trạng thái ngưỡng
     ------------------------------------------------------------------------ */
  // Chưa cho phép điều khiển LED cho đến khi ghi ngưỡng thành công.
  thresholdInitialized = false;

  Serial.println("Waiting to initialize Firebase threshold...");
}

/* ==========================================================================
   SECTION 5: Vòng lặp chính
   ========================================================================== */
void loop() {

  /* ------------------------------------------------------------------------
     SECTION 5.1: Ghi ngưỡng mặc định khi khởi động
     ------------------------------------------------------------------------ */
  if (Firebase.ready() && !thresholdInitialized) {

    Serial.println("Initializing /test/float = 1.5");

    if (Firebase.RTDB.setFloat(
          &fbdo,
          "/test/float",
          INITIAL_THRESHOLD)) {

      floatValue = INITIAL_THRESHOLD;
      thresholdInitialized = true;
      lastFirebaseRead = millis();

      Serial.println("Initialization successful.");
      Serial.printf(
        "Threshold: %.2f V\n",
        floatValue
      );

    } else {
      Serial.printf(
        "Initialization failed: %s\n",
        fbdo.errorReason().c_str()
      );
    }
  }

  /* ------------------------------------------------------------------------
     SECTION 5.2: Đọc ngưỡng từ Firebase mỗi 15 giây
     ------------------------------------------------------------------------ */
  else if (
    Firebase.ready() &&
    thresholdInitialized &&
    millis() - lastFirebaseRead >= FIREBASE_INTERVAL
  ) {
    lastFirebaseRead = millis();

    if (Firebase.RTDB.getFloat(&fbdo, "/test/float")) {
      floatValue = fbdo.floatData();

      Serial.printf(
        "Updated threshold: %.3f V\n",
        floatValue
      );

    } else {
      Serial.printf(
        "Read failed: %s\n",
        fbdo.errorReason().c_str()
      );
    }
  }

  /* ------------------------------------------------------------------------
     SECTION 5.3: Đọc cảm biến quang trở GPIO39
     ------------------------------------------------------------------------ */
  sensorRaw = analogRead(LDR_PIN);

  // Quy đổi gần đúng ADC 12 bit sang điện áp 0–3.3 V.
  sensorVoltage = sensorRaw * (3.3f / 4095.0f);

  /* ------------------------------------------------------------------------
     SECTION 5.4: Điều khiển LED GPIO2
     ------------------------------------------------------------------------ */
  if (!thresholdInitialized) {
    // Chưa ghi thành công ngưỡng Firebase: giữ LED tắt.
    digitalWrite(LED_PIN, LOW);
  } else if (sensorVoltage < floatValue) {
    // Giả định che quang trở làm điện áp giảm.
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  /* ------------------------------------------------------------------------
     SECTION 5.5: In dữ liệu debug
     ------------------------------------------------------------------------ */
  Serial.printf(
    "ADC: %d | Voltage: %.3f V | Threshold: %.3f V | LED: %s\n",
    sensorRaw,
    sensorVoltage,
    floatValue,
    (thresholdInitialized && sensorVoltage < floatValue)
      ? "ON"
      : "OFF"
  );

  delay(100);
}