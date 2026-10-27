// path: src/main.cpp

/* ==========================================================================
   SECTION 1: Nap cac thu vien can thiet
   ========================================================================== */
#include <Arduino.h>
#include <Firebase_ESP_Client.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

/* ==========================================================================
   SECTION 2: Nap cac helper addons (mobizt Firebase_ESP_Client)
   ========================================================================== */
// Token generation process info
#include "addons/TokenHelper.h"
// RTDB payload printing + helper functions
#include "addons/RTDBHelper.h"

#include "secrets.h"  // WIFI_SSID, WIFI_PASSWORD, API_KEY, DATABASE_URL,
                      // USER_EMAIL, USER_PASSWORD

/* ==========================================================================
   SECTION 3: Firebase RTDB paths (ECS 5-Tier Architecture)
   ========================================================================== */
#define TIER02_PATH  "pet_feeder/station_01/02_append_log"
#define TIER03_PATH  "pet_feeder/station_01/03_user_progress"
#define TIER05_PATH  "pet_feeder/station_01/05_user_config"

/* ==========================================================================
   SECTION 4: Firebase objects & control flags
   ========================================================================== */
FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

bool signupOK = false;

/* ==========================================================================
   SECTION 5: ADC pins (GPIO 34, 35, 36, 39 — input-only on ESP32)
              All 4 share one potentiometer -> same raw sweep, different
              output ranges so each field occupies its physical unit space:
                GPIO 34 -> bowlWater  : 0 - 250  ml  (bowl water capacity)
                GPIO 35 -> bowlFood   : 0 - 150  g   (bowl food capacity)
                GPIO 36 -> waterTank  : 0 - 3000 ml  (tank water capacity)
                GPIO 39 -> foodTank   : 0 - 2500 g   (tank food capacity)
   ========================================================================== */
static const int ADC_PINS[4] = { 34, 35, 36, 39 };

// Per-field ADC ceiling values (raw 4095 maps to these maxima)
static const int ADC_MAX_BOWL_WATER  =  250;   // ml
static const int ADC_MAX_BOWL_FOOD   =  150;   // g
static const int ADC_MAX_WATER_TANK  = 3000;   // ml
static const int ADC_MAX_FOOD_TANK   = 2500;   // g

/* ==========================================================================
   SECTION 6: Local mirror of 05_user_config thresholds
   ========================================================================== */
struct UserConfig {
  float tempMin      = 16.0f;
  float tempMax      = 28.0f;
  float moistureMax  = 65.0f;
  int   bowlWaterMin = 50;
  int   bowlWaterMax = 200;
  int   bowlFoodMin  = 30;
  int   bowlFoodMax  = 100;
};
UserConfig gConfig;

/* ==========================================================================
   SECTION 7: Local mirror of last sensor readings (for autoFill logic)
   ========================================================================== */
struct SensorSnapshot {
  float bowlWater = 140.0f;
  float bowlFood  = 45.0f;
  float waterTank = 2200.0f;
  float foodTank  = 1850.0f;
};
SensorSnapshot gSensors;

// autoFill state published to 03_user_progress
bool gAutoFillWater = true;
bool gAutoFillFood  = true;

/* ==========================================================================
   SECTION 8: Timing constants
   ========================================================================== */
static const unsigned long UPLOAD_INTERVAL_MS = 5000UL;   // ADC -> Firebase every 5 s
static const unsigned long CONFIG_POLL_MS     = 30000UL;  // re-read config every 30 s
unsigned long lastUploadMs = 0;
unsigned long lastConfigMs = 0;

/* ==========================================================================
   SECTION 9: Helper — map raw ADC 0-4095 to 0.00-maxVal (float, per-field)
   ========================================================================== */
inline float adcMap(int raw, int maxVal) {
  return roundf((float)raw * maxVal / 4095.0f * 100.0f) / 100.0f;
}

/* ==========================================================================
   SECTION 10: Read ADC pin with averaging, return raw 0-4095
   ========================================================================== */
int readADCRaw(int pin, int samples = 8) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return (int)(sum / samples);
}

/* ==========================================================================
   SECTION 11: Parse 05_user_config JSON and update gConfig
   ========================================================================== */
void parseConfig(FirebaseJson &json) {
  FirebaseJsonData result;
  if (json.get(result, "config/tempMin"))      gConfig.tempMin      = result.floatValue;
  if (json.get(result, "config/tempMax"))      gConfig.tempMax      = result.floatValue;
  if (json.get(result, "config/moistureMax"))  gConfig.moistureMax  = result.floatValue;
  if (json.get(result, "config/bowlWaterMin")) gConfig.bowlWaterMin = result.intValue;
  if (json.get(result, "config/bowlWaterMax")) gConfig.bowlWaterMax = result.intValue;
  if (json.get(result, "config/bowlFoodMin"))  gConfig.bowlFoodMin  = result.intValue;
  if (json.get(result, "config/bowlFoodMax"))  gConfig.bowlFoodMax  = result.intValue;

  Serial.printf("[Config] tempMin=%.1f tempMax=%.1f moistMax=%.1f "
                "bowlWater[%d-%d] bowlFood[%d-%d]\n",
                gConfig.tempMin, gConfig.tempMax, gConfig.moistureMax,
                gConfig.bowlWaterMin, gConfig.bowlWaterMax,
                gConfig.bowlFoodMin, gConfig.bowlFoodMax);
}

/* ==========================================================================
   SECTION 12: Fetch 05_user_config from Firebase RTDB
   ========================================================================== */
void fetchConfig() {
  if (Firebase.RTDB.getJSON(&fbdo, TIER05_PATH)) {
    FirebaseJson &json = fbdo.jsonObject();
    parseConfig(json);
  } else {
    Serial.println("FAILED fetchConfig");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

/* ==========================================================================
   SECTION 13: Evaluate autoFill rules, write to 03_user_progress if changed
               Logic mirrors feeder-service.js checkAutoRefillRules():
                 bowlWater <= bowlWaterMin  =>  autoFillWater = true
                 bowlWater >= bowlWaterMax  =>  autoFillWater = false
                 (same for food)
               Only writes Firebase when state actually changes.
   ========================================================================== */
void evaluateAndPublishAutoFill() {
  bool newAutoWater = gAutoFillWater;
  bool newAutoFood  = gAutoFillFood;

  if (gSensors.bowlWater <= (float)gConfig.bowlWaterMin) {
    newAutoWater = true;
  } else if (gSensors.bowlWater >= (float)gConfig.bowlWaterMax) {
    newAutoWater = false;
  }

  if (gSensors.bowlFood <= (float)gConfig.bowlFoodMin) {
    newAutoFood = true;
  } else if (gSensors.bowlFood >= (float)gConfig.bowlFoodMax) {
    newAutoFood = false;
  }

  if (newAutoWater != gAutoFillWater || newAutoFood != gAutoFillFood) {
    gAutoFillWater = newAutoWater;
    gAutoFillFood  = newAutoFood;

    FirebaseJson progress;
    progress.set("autoFillWater", gAutoFillWater);
    progress.set("autoFillFood",  gAutoFillFood);

    if (Firebase.RTDB.updateNode(&fbdo, TIER03_PATH, &progress)) {
      Serial.printf("[Progress] autoFillWater=%s autoFillFood=%s -> PASSED\n",
                    gAutoFillWater ? "true" : "false",
                    gAutoFillFood  ? "true" : "false");
      Serial.println("PATH: " + fbdo.dataPath());
    } else {
      Serial.println("FAILED evaluateAndPublishAutoFill");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

/* ==========================================================================
   SECTION 14: Read all 4 ADC pins, upload to 02_append_log
              Same pot sweep (0-4095 raw) -> different output ranges per field
   ========================================================================== */
void uploadSensorReadings() {
  // Read raw then map each field to its own physical range (float)
  int raw34 = readADCRaw(ADC_PINS[0]);
  int raw35 = readADCRaw(ADC_PINS[1]);
  int raw36 = readADCRaw(ADC_PINS[2]);
  int raw39 = readADCRaw(ADC_PINS[3]);

  float bowlWater = adcMap(raw34, ADC_MAX_BOWL_WATER);  // 0.00-250.00  ml
  float bowlFood  = adcMap(raw35, ADC_MAX_BOWL_FOOD);   // 0.00-150.00  g
  float waterTank = adcMap(raw36, ADC_MAX_WATER_TANK);  // 0.00-3000.00 ml
  float foodTank  = adcMap(raw39, ADC_MAX_FOOD_TANK);   // 0.00-2500.00 g

  Serial.printf("[ADC raw] %d %d %d %d\n", raw34, raw35, raw36, raw39);
  Serial.printf("[Mapped ] bowlWater=%.2f ml  bowlFood=%.2f g  "
                "waterTank=%.2f ml  foodTank=%.2f g\n",
                bowlWater, bowlFood, waterTank, foodTank);

  // Mirror locally for autoFill evaluation
  gSensors.bowlWater = bowlWater;
  gSensors.bowlFood  = bowlFood;
  gSensors.waterTank = waterTank;
  gSensors.foodTank  = foodTank;

  // Build JSON patch for 02_append_log — all values as float
  FirebaseJson sensors;
  sensors.set("bowlWater", bowlWater);
  sensors.set("bowlFood",  bowlFood);
  sensors.set("waterTank", waterTank);
  sensors.set("foodTank",  foodTank);

  if (Firebase.RTDB.updateNode(&fbdo, TIER02_PATH, &sensors)) {
    Serial.println("PASSED uploadSensorReadings");
    Serial.println("PATH: " + fbdo.dataPath());
  } else {
    Serial.println("FAILED uploadSensorReadings");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

/* ==========================================================================
   SECTION 15: setup()
   ========================================================================== */
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Boot] ESP32 Firebase Bridge — sketch_oct4a");

  // GPIO 34/35/36/39 are input-only by hardware
  for (int pin : ADC_PINS) {
    pinMode(pin, INPUT);
  }

  // WiFi
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

  /* -----------------------------------------------------------------------
     SECTION 16: Firebase config — API key + RTDB URL
     ----------------------------------------------------------------------- */
  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;

  /* -----------------------------------------------------------------------
     SECTION 17: Firebase auth — Email / Password (from secrets.h)
     NOTE: No Firebase.signUp() needed; direct email/password login.
           signupOK = true unlocks the loop() send cycle.
     ----------------------------------------------------------------------- */
  auth.user.email    = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  signupOK = true;

  /* -----------------------------------------------------------------------
     SECTION 18: Token callback + Firebase.begin()
     ----------------------------------------------------------------------- */
  config.token_status_callback = tokenStatusCallback; // addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.print("Waiting for Firebase...");
  while (!Firebase.ready()) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\n[Firebase] Ready");

  // Initial config read
  fetchConfig();
  lastConfigMs = millis();
}

/* ==========================================================================
   SECTION 19: loop()
   ========================================================================== */
void loop() {
  unsigned long now = millis();

  /* -----------------------------------------------------------------------
     SECTION 20: Upload ADC readings every UPLOAD_INTERVAL_MS (5 s)
     ----------------------------------------------------------------------- */
  if (Firebase.ready() && signupOK &&
      (now - lastUploadMs > UPLOAD_INTERVAL_MS || lastUploadMs == 0)) {
    lastUploadMs = now;

    uploadSensorReadings();
    evaluateAndPublishAutoFill();
  }

  /* -----------------------------------------------------------------------
     SECTION 21: Re-read 05_user_config every CONFIG_POLL_MS (30 s)
     ----------------------------------------------------------------------- */
  if (Firebase.ready() && signupOK &&
      (now - lastConfigMs > CONFIG_POLL_MS)) {
    lastConfigMs = now;
    fetchConfig();
  }
}
