// ======================================================
//          CNG PROTECT – FIREBASE NATIVE EDITION
//      Monitors MQ2 & DHT11 -> Syncs with Firebase RTDB
// ======================================================

#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <time.h> 
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// *** FIREBASE LIBRARIES ***
#include <Firebase_ESP_Client.h> 
#include "addons/TokenHelper.h" 
#include "addons/RTDBHelper.h"

// --------------------- DEBUG TOGGLE -----------------------------
// Set to 1 to enable detailed serial logging, 0 to disable for production
#define DEBUG_MODE 0 

#if DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// --------------------- FIREBASE CREDENTIALS ---------------------
#define API_KEY         "XXXX"
#define DATABASE_URL    "https://cng-protect-default-rtdb.firebaseio.com/" 
#define USER_EMAIL      "victorogbonna313@gmail.com"
#define USER_PASSWORD   "CNG1234."

// --------------------- NETWORK CONFIG ---------------------
#define WIFI_SSID       "Redmi Victor"
#define WIFI_PASSWORD   "kingobi2022"

// --------------------- PINS ---------------------
#define MQ2_PIN         34
#define DHT_PIN         4
#define DHT_TYPE        DHT11

// --------------------- OBJECTS ---------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2); 
DHT dht(DHT_PIN, DHT_TYPE);

// Firebase Objects
FirebaseData fbDO;
FirebaseAuth auth;
FirebaseConfig config;

// FreeRTOS & Sync
SemaphoreHandle_t sensorDataMutex; 

// --------------------- CUSTOM LCD CHARACTERS ----------------
byte upArrow[8] = { 0b00100, 0b01110, 0b10101, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000 };
byte downArrow[8] = { 0b00100, 0b00100, 0b00100, 0b00100, 0b10101, 0b01110, 0b00100, 0b00000 };

// --------------------- SHARED DATA ------------------------
volatile float temperature = 0.0; 
volatile int gasValue = 0;
volatile bool isDanger = false;
volatile bool isOnline = false;

// Default Thresholds
float tempThreshold = 40.0;
int gasThreshold = 1100;

// Timers
unsigned long lastPublishTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSettingsCheck = 0;
unsigned long lastDebugPrint = 0; // Timer for serial output
bool firebaseConfigured = false;

// --------------------- PROTOTYPES -----------------------
void connectWiFi();
void syncTime();
void setupFirebase();
void updateLCD();

// --------------------- SETUP -----------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  DEBUG_PRINTLN("\n--- CNG PROTECT BOOTING ---");
  DEBUG_PRINTLN("Initializing Hardware...");
  
  // 1. Hardware Init
  lcd.init(); lcd.backlight();
  lcd.createChar(0, upArrow);   
  lcd.createChar(1, downArrow); 
  
  lcd.setCursor(0,0); lcd.print("CNG PROTECT");
  lcd.setCursor(0,1); lcd.print("System Boot...");
  dht.begin();
  
  // 2. FreeRTOS Init
  sensorDataMutex = xSemaphoreCreateMutex(); 

  // 3. Network Setup
  connectWiFi();
  
  // 4. Time Sync 
  if(WiFi.status() == WL_CONNECTED) syncTime();
  
  // 5. Firebase Setup
  setupFirebase();

  lcd.clear();
  lcd.print("System Ready!");
  DEBUG_PRINTLN("--- SYSTEM READY ---");
  delay(1000);
}

// --------------------- MAIN LOOP ------------------------
void loop() {
  
  // 1. Read Sensors
  float temp = dht.readTemperature();
  int gas = analogRead(MQ2_PIN); 

  // 2. Update Shared Data (Thread Safe)
  if (xSemaphoreTake(sensorDataMutex, 0) == pdTRUE) {
    if (!isnan(temp) && temp > -50) temperature = temp;
    gasValue = gas;
    
    bool previousDangerState = isDanger;
    isDanger = (temperature > tempThreshold) || (gasValue > gasThreshold);
    
    if(isDanger != previousDangerState) {
        if(isDanger) {
            DEBUG_PRINTLN("\n[!] ALERT: THRESHOLD BREACHED. SWITCHING TO DANGER MODE [!]");
        } else {
            DEBUG_PRINTLN("\n[*] ALERT RESOLVED: Hardware returning to SAFE MODE [*]");
        }
    }
    xSemaphoreGive(sensorDataMutex);
  }

  // Debug Print Sensor Data (Every 1 second)
  if (millis() - lastDebugPrint >= 1000) {
      lastDebugPrint = millis();
      DEBUG_PRINTF("[SENSOR] Gas: %d (Limit: %d) | Temp: %.1fC (Limit: %.1fC) | State: %s\n", 
                   gasValue, gasThreshold, temperature, tempThreshold, 
                   isDanger ? "DANGER" : "SAFE");
  }

  // 3. PUSH to Firebase (Every 2 Seconds)
  if (firebaseConfigured && (millis() - lastPublishTime >= 2000)) {
    lastPublishTime = millis();
    
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
       FirebaseJson json;
       json.set("temperature", temperature);
       json.set("gas_level", gasValue);
       json.set("is_danger", isDanger);
       json.set("timestamp", millis()); 

       DEBUG_PRINT("[FIREBASE] Pushing Data... ");
       if (Firebase.RTDB.setJSON(&fbDO, "/cng_protect/devices/device_01/live_data", &json)) {
         isOnline = true; 
         DEBUG_PRINTLN("SUCCESS");
       } else {
         isOnline = false; 
         DEBUG_PRINT("FAILED: "); DEBUG_PRINTLN(fbDO.errorReason());
       }
    } else {
       isOnline = false;
       DEBUG_PRINTLN("[NETWORK] WiFi or Firebase disconnected.");
    }
  }

  // 4. PULL Settings from Firebase (Every 10 Seconds)
  if (firebaseConfigured && isOnline && (millis() - lastSettingsCheck >= 10000)) {
    lastSettingsCheck = millis();
    DEBUG_PRINT("[FIREBASE] Checking for new cloud limits... ");
    
    if (Firebase.RTDB.getInt(&fbDO, "/cng_protect/devices/device_01/settings/gas_limit")) {
      if (fbDO.dataType() == "int") {
        int newLimit = fbDO.intData();
        if (newLimit > 0 && newLimit != gasThreshold) {
          gasThreshold = newLimit;
          DEBUG_PRINTF("\n[UPDATE] New Gas Limit applied from Cloud: %d\n", gasThreshold);
        } else {
            DEBUG_PRINTLN("No changes.");
        }
      }
    } else {
        DEBUG_PRINTLN("No settings found.");
    }
  }

  // 5. LCD Update
  if (millis() - lastDisplayUpdate >= 500) {
    updateLCD();
    lastDisplayUpdate = millis();
  }
  
  vTaskDelay(10); 
}

// --------------------- FIREBASE SETUP ------------------------
void setupFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("[ERROR] Cannot connect to Firebase without WiFi.");
    firebaseConfigured = false;
    return;
  }

  DEBUG_PRINTF("[FIREBASE] Init Client v%s\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback; 
  fbDO.setResponseSize(2048); 
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  DEBUG_PRINT("[FIREBASE] Waiting for Token Generation");
  unsigned long start = millis();
  while (!Firebase.ready() && millis() - start < 10000) {
    delay(200);
    DEBUG_PRINT(".");
  }
  
  if (Firebase.ready()) {
    DEBUG_PRINTLN(" READY!");
    firebaseConfigured = true;
    isOnline = true;
  } else {
    DEBUG_PRINTLN(" FAILED!");
    DEBUG_PRINTLN(fbDO.errorReason());
    firebaseConfigured = false;
  }
}

// --------------------- HELPERS ------------------------
void connectWiFi() {
  DEBUG_PRINTF("[WIFI] Connecting to %s ", WIFI_SSID);
  lcd.setCursor(0,1); lcd.print("WiFi...         ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) { 
    delay(300); DEBUG_PRINT(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
      DEBUG_PRINTLN(" CONNECTED!");
      DEBUG_PRINT("[WIFI] IP Address: "); DEBUG_PRINTLN(WiFi.localIP());
  } else {
      DEBUG_PRINTLN(" FAILED (Offline Mode)");
  }
}

void syncTime() {
  DEBUG_PRINT("[TIME] Syncing via NTP... ");
  configTime(3600, 0, "pool.ntp.org", "time.nist.gov");
  unsigned long start = millis();
  while (time(nullptr) < 1000000000 && millis() - start < 5000) { 
    delay(500);
  }
  DEBUG_PRINTLN("DONE.");
}

void updateLCD() {
  lcd.clear();
  
  if (isDanger) {
    lcd.setCursor(0,0); lcd.print(">> DANGER <<");
  } else {
    lcd.setCursor(0,0); lcd.print("SAFE MODE");
  }

  lcd.setCursor(15, 0);
  if (isOnline) {
    lcd.write(0); 
  } else {
    lcd.write(1); 
  }

  lcd.setCursor(0,1); 
  lcd.print("G:"); lcd.print(gasValue); 
  lcd.print(" T:"); lcd.print((int)temperature);
}
