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
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#include "secrets.h"

/* ==========================================================================
   SECTION 3: Khai báo thông tin mạng Wi-Fi
   ========================================================================== */
// Đã khai báo trong secrets.h (WIFI_SSID, WIFI_PASSWORD)

/* ==========================================================================
   SECTION 4: Khai báo Firebase API Key
   ========================================================================== */
// Đã khai báo trong secrets.h (API_KEY)

/* ==========================================================================
   SECTION 5: Khai báo Firebase Realtime Database URL
   ========================================================================== */
// Đã khai báo trong secrets.h (DATABASE_URL)

/* ==========================================================================
   SECTION 6: Khởi tạo đối tượng Firebase và các biến điều khiển
   ========================================================================== */
//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

int intValue;
float floatValue;

void setup() {
  /* ========================================================================
     SECTION 7: Khởi tạo Serial và kết nối Wi-Fi
     ======================================================================== */
  Serial.begin(115200);
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
     SECTION 8: Gán API Key và RTDB URL vào cấu hình
     ======================================================================== */
  /* Assign the api key (required) */
  config.api_key = API_KEY;
  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* ========================================================================
     SECTION 9: Đăng ký / Xác thực người dùng
     ======================================================================== */
  // Gán thông tin xác thực Email/Password từ secrets.h
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  signupOK = true;
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     [FIXED CRITICAL ERROR - SECTION 9]:
     - ĐÃ LOẠI BỎ: Firebase.signUp(&config, &auth, "", "") gây lỗi 400 INVALID_EMAIL 
       do GITKit từ chối email rỗng khi Anonymous Auth chưa được cấu hình.
     - ĐÃ THAY THẾ: Gán auth.user.email và auth.user.password từ secrets.h để thư viện 
       tự động đăng nhập hợp lệ với Email/Password đã đăng ký trên Firebase Console.
     - Đặt signupOK = true để Section 11 mở khóa luồng loop().
     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

  /* ========================================================================
     SECTION 10: Gán Callback kiểm tra Token và khởi động Firebase
     ======================================================================== */
  /* Assign the callback function for
  the long running token generation task */
  config.token_status_callback = tokenStatusCallback;
  //see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  /* ========================================================================
     SECTION 11: Kiểm tra trạng thái và chu kỳ gửi dữ liệu (mỗi 15 giây)
     ======================================================================== */
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {

    /* ======================================================================
       SECTION 12: Cập nhật mốc thời gian gửi dữ liệu
       ====================================================================== */
    sendDataPrevMillis = millis();

    /* ======================================================================
       SECTION 13: Mã lệnh ghi kiểu int được thay thế bằng mã lệnh đọc dữ liệu từ test/int 
       ====================================================================== */

    if (Firebase.RTDB.getInt(&fbdo, "/test/int")) {
    if (fbdo.dataType() == "int") {
    intValue = fbdo.intData();
    Serial.println(intValue);
    }
    } else {
    Serial.println(fbdo.errorReason());
    }

    /* ======================================================================
       SECTION 14: Ghi số thực ngẫu nhiên lên path test/float
       ====================================================================== */
    // Write an Float number on the database path test/float
if (Firebase.RTDB.getFloat(&fbdo, "/test/float")) {
 if (fbdo.dataType() == "float") {
 floatValue = fbdo.floatData();
 Serial.println(floatValue);
 }
} else {
 Serial.println(fbdo.errorReason());
}
  }
}