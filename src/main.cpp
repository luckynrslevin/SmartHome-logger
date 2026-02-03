/****************************************************
 *  ESP8266/ESP32 + DS18B20 + Adafruit IO
 *  Hardened for unstable WiFi
 *  - LittleFS persistent storage
 *  - RSSI logging
 *  - Watchdog safe
 *  - Dynamic sensor detection (0-10 sensors)
 ****************************************************/

#if defined(ESP32)
  #include <WiFi.h>
  #include <LittleFS.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <LittleFS.h>
#else
  #error "Unsupported board. Use ESP8266 or ESP32."
#endif

#include <OneWire.h>
#include <DallasTemperature.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/* =========================================================
   USER CONFIGURATION
   Copy config.h.example to config.h and fill in your values
   ========================================================= */

#include "config.h"

// Verify config.h is set up
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
  #error "Missing WiFi config. Copy config.h.example to config.h and fill in your values."
#endif

#if !defined(AIO_USERNAME) || !defined(AIO_KEY)
  #error "Missing Adafruit IO config. Copy config.h.example to config.h and fill in your values."
#endif

// Adafruit IO server settings
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883

/* ========================================================= */

// Publish interval (ms)
#define PUBLISH_INTERVAL 60000  // 1 minute

// DS18B20
#if defined(ESP32)
  #define ONE_WIRE_BUS 4   // GPIO4 on ESP32
#else
  #define ONE_WIRE_BUS D4  // D4 on ESP8266
#endif
#define MAX_SENSORS 10

// Filesystem
#define DATA_FILE "/temps.csv"

/* =========================================================
   MQTT
   ========================================================= */

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT,
                          AIO_USERNAME, AIO_KEY);

/* =========================================================
   DS18B20
   ========================================================= */

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress sensorAddresses[MAX_SENSORS];
uint8_t sensorCount = 0;
float temperatures[MAX_SENSORS];

unsigned long lastPublish = 0;

/* =========================================================
   HELPER: Print sensor address
   ========================================================= */

void printAddress(DeviceAddress addr) {
  for (uint8_t i = 0; i < 8; i++) {
    if (addr[i] < 16) Serial.print("0");
    Serial.print(addr[i], HEX);
  }
}

/* =========================================================
   HELPER: Build MQTT feed path
   ========================================================= */

bool publishToFeed(uint8_t sensorIndex, float value) {
  char feedPath[64];
  char valueStr[16];
  snprintf(feedPath, sizeof(feedPath), "%s/feeds/temp_sensor_%d",
           AIO_USERNAME, sensorIndex + 1);
  dtostrf(value, 1, 2, valueStr);
  return mqtt.publish(feedPath, valueStr);
}

/* =========================================================
   FILESYSTEM
   ========================================================= */

void initFS() {
#if defined(ESP32)
  if (!LittleFS.begin(true)) {  // true = format on fail
#else
  if (!LittleFS.begin()) {
#endif
    Serial.println("LittleFS mount FAILED");
    return;
  }
  Serial.println("LittleFS mounted");

  // Create data file if it doesn't exist (avoids ESP32 VFS error logs)
  File f = LittleFS.open(DATA_FILE, "a");
  if (f) f.close();
}

bool hasStoredData() {
  File f = LittleFS.open(DATA_FILE, "r");
  if (!f) return false;
  bool hasData = f.size() > 0;
  f.close();
  return hasData;
}

void storeToFS(float temps[], uint8_t count) {
  int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

  File f = LittleFS.open(DATA_FILE, "a");
  if (!f) {
    Serial.println("FS open failed");
    return;
  }

  // Format: millis,count,temp1,temp2,...,tempN,rssi
  f.printf("%lu,%d", millis(), count);
  for (uint8_t i = 0; i < count; i++) {
    f.printf(",%.2f", temps[i]);
  }
  f.printf(",%d\n", rssi);
  f.close();

  Serial.println("Measurement stored to FS");
}

/* =========================================================
   WIFI / MQTT HARDENING
   ========================================================= */

bool ensureWiFi(uint32_t timeoutMs = 30000) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.println("WiFi reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("WiFi timeout");
      return false;
    }
    delay(250);
  }

  Serial.println("WiFi connected");
  return true;
}

bool ensureMQTT(uint32_t timeoutMs = 10000) {
  if (mqtt.connected()) return true;
  if (WiFi.status() != WL_CONNECTED) return false;

  uint32_t start = millis();
  while (!mqtt.connected()) {
    Serial.print("MQTT connecting...");
    if (mqtt.connect() == 0) {
      Serial.println("connected");
      return true;
    }

    Serial.println("failed");
    mqtt.disconnect();

    if (millis() - start > timeoutMs) {
      Serial.println("MQTT timeout");
      return false;
    }

    delay(1000);
  }
  return true;
}

/* =========================================================
   PUBLISH STORED DATA
   ========================================================= */

void publishStoredData() {
  if (!ensureWiFi() || !ensureMQTT()) return;

  File src = LittleFS.open(DATA_FILE, "r");
  if (!src) return;

  File tmp = LittleFS.open("/tmp.csv", "w");
  if (!tmp) {
    src.close();
    return;
  }

  while (src.available()) {
    String line = src.readStringUntil('\n');
    if (line.length() < 5) continue;

    // Parse: millis,count,temp1,...,tempN,rssi
    int pos = 0;
    int nextComma;

    // Skip millis
    nextComma = line.indexOf(',', pos);
    if (nextComma < 0) continue;
    pos = nextComma + 1;

    // Get count
    nextComma = line.indexOf(',', pos);
    if (nextComma < 0) continue;
    uint8_t count = line.substring(pos, nextComma).toInt();
    pos = nextComma + 1;

    if (count == 0 || count > MAX_SENSORS) continue;

    // Get temperatures
    float temps[MAX_SENSORS];
    for (uint8_t i = 0; i < count; i++) {
      nextComma = line.indexOf(',', pos);
      if (nextComma < 0) break;
      temps[i] = line.substring(pos, nextComma).toFloat();
      pos = nextComma + 1;
    }

    // Get RSSI (remaining after last comma)
    int rssi = line.substring(pos).toInt();

    Serial.printf("Publishing %d sensors (RSSI %d)\n", count, rssi);

    bool publishOk = true;
    for (uint8_t i = 0; i < count && publishOk; i++) {
      Serial.printf("  Sensor %d: %.2f °C\n", i + 1, temps[i]);
      if (!publishToFeed(i, temps[i])) {
        publishOk = false;
      }
      mqtt.processPackets(10);
      delay(50);
    }

    if (!publishOk) {
      // re-queue unsent data
      tmp.println(line);
      tmp.print(src.readString());
      break;
    }

    mqtt.ping();
  }

  src.close();
  tmp.close();

  LittleFS.remove(DATA_FILE);
  LittleFS.rename("/tmp.csv", DATA_FILE);
}

/* =========================================================
   SENSOR DISCOVERY
   ========================================================= */

void discoverSensors() {
  sensors.begin();

  sensorCount = sensors.getDeviceCount();
  if (sensorCount > MAX_SENSORS) {
    sensorCount = MAX_SENSORS;
  }

  Serial.printf("DS18B20 sensors found: %d\n", sensorCount);

  for (uint8_t i = 0; i < sensorCount; i++) {
    if (sensors.getAddress(sensorAddresses[i], i)) {
      sensors.setResolution(sensorAddresses[i], 12);
      Serial.printf("  Sensor %d: ", i + 1);
      printAddress(sensorAddresses[i]);
      Serial.println();
    }
  }
}

/* =========================================================
   SETUP
   ========================================================= */

void setup() {
  Serial.begin(115200);
  delay(1000);

#if defined(ESP32)
  Serial.println("\nESP32 DS18B20 Logger");
#else
  Serial.println("\nESP8266 DS18B20 Logger");
#endif

  initFS();
  ensureWiFi();
  discoverSensors();

  Serial.println("Setup complete");
}

/* =========================================================
   LOOP
   ========================================================= */

void loop() {
  unsigned long now = millis();

  // --- Measurement cycle ---
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;

    if (sensorCount == 0) {
      Serial.println("No sensors connected");
    } else {
      sensors.requestTemperatures();

      bool allValid = true;
      Serial.print("Measured: ");

      for (uint8_t i = 0; i < sensorCount; i++) {
        temperatures[i] = sensors.getTempC(sensorAddresses[i]);
        if (temperatures[i] == DEVICE_DISCONNECTED_C) {
          allValid = false;
        }
        if (i > 0) Serial.print(" / ");
        Serial.printf("%.2f", temperatures[i]);
      }
      Serial.println(" °C");

      if (allValid) {
        storeToFS(temperatures, sensorCount);
      } else {
        Serial.println("Sensor error - some readings invalid");
      }
    }
  }

  // --- Upload backlog ---
  if (hasStoredData()) {
    publishStoredData();
  }

  mqtt.processPackets(10);
  mqtt.ping();

  delay(100);  // Prevent tight loop
}
