/****************************************************
 *  ESP8266 + DS18B20 + Adafruit IO
 *  Hardened for unstable WiFi
 *  - LittleFS persistent storage
 *  - RSSI logging
 *  - Watchdog safe
 ****************************************************/

#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LittleFS.h>

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
#define ONE_WIRE_BUS D4
#define SENSOR_COUNT 2

// Filesystem
#define DATA_FILE "/temps.csv"

// Watchdog helper
#define WDT_FEED() yield()

/* =========================================================
   MQTT
   ========================================================= */

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT,
                          AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish tempFeed1 =
  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temp_sensor_1");

Adafruit_MQTT_Publish tempFeed2 =
  Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temp_sensor_2");

/* =========================================================
   DS18B20
   ========================================================= */

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress sensorAddresses[SENSOR_COUNT] = {
  {0x28, 0xE1, 0x17, 0x32, 0x00, 0x00, 0x00, 0x62},
  {0x28, 0x8F, 0xBF, 0x31, 0x00, 0x00, 0x00, 0x37}
};

unsigned long lastPublish = 0;

/* =========================================================
   FILESYSTEM
   ========================================================= */

void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount FAILED");
  } else {
    Serial.println("LittleFS mounted");
  }
}

bool hasStoredData() {
  if (!LittleFS.exists(DATA_FILE)) return false;
  File f = LittleFS.open(DATA_FILE, "r");
  bool hasData = f && f.size() > 0;
  f.close();
  return hasData;
}

void storeToFS(float t1, float t2) {
  int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

  File f = LittleFS.open(DATA_FILE, "a");
  if (!f) {
    Serial.println("FS open failed");
    return;
  }

  // millis,temp1,temp2,rssi
  f.printf("%lu,%.2f,%.2f,%d\n", millis(), t1, t2, rssi);
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
    WDT_FEED();
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
    WDT_FEED();
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
    WDT_FEED();

    String line = src.readStringUntil('\n');
    if (line.length() < 5) continue;

    unsigned long ts;
    float t1, t2;
    int rssi;

    if (sscanf(line.c_str(), "%lu,%f,%f,%d",
               &ts, &t1, &t2, &rssi) != 4) {
      continue;
    }

    Serial.printf("Publishing %.2f / %.2f °C (RSSI %d)\n",
                  t1, t2, rssi);

    if (!tempFeed1.publish(t1) ||
        !tempFeed2.publish(t2)) {
      // re-queue unsent data
      tmp.println(line);
      tmp.print(src.readString());
      break;
    }

    mqtt.processPackets(10);
    mqtt.ping();
    delay(50);
  }

  src.close();
  tmp.close();

  LittleFS.remove(DATA_FILE);
  LittleFS.rename("/tmp.csv", DATA_FILE);
}

/* =========================================================
   SETUP
   ========================================================= */

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nESP8266 DS18B20 Logger");

  initFS();
  ensureWiFi();

  sensors.begin();
  sensors.setResolution(12);

  Serial.print("DS18B20 devices found: ");
  Serial.println(sensors.getDeviceCount());

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

    sensors.requestTemperatures();

    float t1 = sensors.getTempC(sensorAddresses[0]);
    float t2 = sensors.getTempC(sensorAddresses[1]);

    if (t1 != DEVICE_DISCONNECTED_C &&
        t2 != DEVICE_DISCONNECTED_C) {

      Serial.printf("Measured: %.2f / %.2f °C\n", t1, t2);
      storeToFS(t1, t2);
    } else {
      Serial.println("Sensor error");
    }
  }

  // --- Upload backlog ---
  if (hasStoredData()) {
    publishStoredData();
  }

  mqtt.processPackets(10);
  mqtt.ping();
  WDT_FEED();
}
