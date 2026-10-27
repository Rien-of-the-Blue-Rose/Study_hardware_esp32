
#include <Arduino.h>
// driver/adc.h không cần thiết trong IDF 5.x (ESP32 Arduino core v3+)
#include <Firebase_ESP_Client.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "secrets.h"

// Mỗi thiết bị ghi dữ liệu dưới UID riêng.
const String DEVICE_PATH = String("devices/") + USER_UID;

// Chân ADC kết nối với ngõ ra quang trở.
const int LDR_PIN = 39;

// Điện áp tham chiếu và độ phân giải ADC.
const float ADC_REFERENCE_VOLTAGE = 3.3;
const int ADC_MAX_VALUE = 4095;

// Firebase objects.
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);

  // GPIO39 là chân ADC1 trên ESP32.
  pinMode(LDR_PIN, INPUT);

  // Cấu hình ADC 12-bit: giá trị đọc từ 0 đến 4095.
  analogReadResolution(12);

  // QUAN TRỌNG: Mặc định ADC_0db chỉ đo 0-1V.
  // ADC_11db mở rộng range lên 0-3.1V — cần thiết cho 2.45V của bạn.
  analogSetAttenuation(ADC_11db);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Cấu hình Firebase.
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.ready() &&
      (millis() - sendDataPrevMillis > 15000 ||
       sendDataPrevMillis == 0)) {

    sendDataPrevMillis = millis();

    Serial.println("Signed in as UID: " +
                   String(auth.token.uid.c_str()));

    // Đọc giá trị ADC từ quang trở.
    int adcValue = analogRead(LDR_PIN);

    // Chuyển đổi giá trị ADC thành điện áp ước tính.
    float voltage = (adcValue * ADC_REFERENCE_VOLTAGE)
                    / ADC_MAX_VALUE;

    Serial.printf("LDR ADC: %d\n", adcValue);
    Serial.printf("LDR Voltage: %.3f V\n", voltage);

    // Ghi điện áp quang trở lên Firebase tại test/float.
    if (Firebase.RTDB.setFloat(
          &fbdo,
          DEVICE_PATH + "/test/float",
          voltage)) {

      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
      Serial.printf("Voltage sent: %.3f V\n", voltage);

    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}
