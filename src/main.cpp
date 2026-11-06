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
  #include <WiFiClientSecure.h>
  #include <LittleFS.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecureBearSSL.h>
  #include <LittleFS.h>
#else
  #error "Unsupported board. Use ESP8266 or ESP32."
#endif

#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include <PubSubClient.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/* =========================================================
   USER CONFIGURATION
   Copy config.h.example to config.h and fill in your values
   ========================================================= */

#include "config.h"

// Verify config.h is set up
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
  #error "Missing WiFi config. Fill in src/config.h."
#endif

#if !defined(AIO_USERNAME) || !defined(AIO_KEY)
  #error "Missing Adafruit IO config. Fill in src/config.h."
#endif
#if !defined(VPS_MQTT_HOST) || !defined(VPS_MQTT_PORT) || \
    !defined(VPS_MQTT_USERNAME) || !defined(VPS_MQTT_PASSWORD) || \
    !defined(VPS_MQTT_TOPIC_PREFIX)
  #error "Missing VPS MQTT settings in src/config.h."
#endif

// Adafruit IO server settings
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883

// Measurement and hardware settings.
#define PUBLISH_INTERVAL 60000UL
#if defined(ESP32)
  #define ONE_WIRE_BUS 4   // GPIO4 on ESP32
#else
  #define ONE_WIRE_BUS D4  // D4 on ESP8266
#endif
#define MAX_SENSORS 10
#define DATA_FILE "/temps.csv"

// Existing Adafruit connection.
WiFiClient aioClient;
Adafruit_MQTT_Client aioMqtt(&aioClient, AIO_SERVER, AIO_SERVERPORT,
                             AIO_USERNAME, AIO_KEY);

// Independent, certificate-validated TLS connection to the VPS broker.
#if defined(ESP8266)
  BearSSL::WiFiClientSecure vpsTlsClient;
  BearSSL::X509List vpsTrustAnchor(VPS_MQTT_CA_CERT);
#else
  WiFiClientSecure vpsTlsClient;
#endif
PubSubClient vpsMqtt(vpsTlsClient);
char vpsClientId[40] = {0};

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorAddresses[MAX_SENSORS];
uint8_t sensorCount = 0;
float temperatures[MAX_SENSORS];
unsigned long lastPublish = 0;
bool firstMeasurementDone = false;  // FR-M-0007: skip first measurement after boot

void printAddress(DeviceAddress addr) {
  for (uint8_t i = 0; i < 8; i++) {
    if (addr[i] < 16) Serial.print("0");
    Serial.print(addr[i], HEX);
  }
}

bool publishToAdafruitFeed(uint8_t sensorIndex, float value) {
  char feedPath[64];
  char valueStr[16];
  snprintf(feedPath, sizeof(feedPath), "%s/feeds/temp_sensor_%d",
           AIO_USERNAME, sensorIndex + 1);
  dtostrf(value, 1, 2, valueStr);
  return aioMqtt.publish(feedPath, valueStr);
}

bool publishToVps(uint8_t sensorIndex, float value) {
  char topic[128];
  char valueStr[20];
  snprintf(topic, sizeof(topic), "%s/sensor_%u",
           VPS_MQTT_TOPIC_PREFIX, static_cast<unsigned>(sensorIndex + 1));
  dtostrf(value, 1, 2, valueStr);
  const bool ok = vpsMqtt.publish(topic, valueStr, false);
  if (!ok) {
    Serial.printf("VPS MQTT publish failed for %s (state %d)\n",
                  topic, vpsMqtt.state());
  }
  return ok;
}

/* ---------------- Filesystem ---------------- */
void initFS() {
#if defined(ESP32)
  if (!LittleFS.begin(true)) {
#else
  if (!LittleFS.begin()) {
#endif
    Serial.println("LittleFS mount FAILED");
    return;
  }
  Serial.println("LittleFS mounted");
  File f = LittleFS.open(DATA_FILE, "a");
  if (f) f.close();
}

bool hasStoredData() {
  File f = LittleFS.open(DATA_FILE, "r");
  if (!f) return false;
  const bool hasData = f.size() > 0;
  f.close();
  return hasData;
}

void storeToFS(float temps[], uint8_t count) {
  const int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  File f = LittleFS.open(DATA_FILE, "a");
  if (!f) {
    Serial.println("FS open failed; measurement was not queued");
    return;
  }
  // Legacy-compatible row: millis,count,temp1,...,tempN,rssi
  // Upload-state flags are appended later as ,adafruitDone,vpsDone.
  f.printf("%lu,%u", millis(), static_cast<unsigned>(count));
  for (uint8_t i = 0; i < count; i++) f.printf(",%.2f", temps[i]);
  f.printf(",%d\n", rssi);
  f.close();
  Serial.println("Measurement stored to LittleFS queue");
}

/* ---------------- Wi-Fi and time ---------------- */
bool ensureWiFi(uint32_t timeoutMs = 30000) {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.println("WiFi reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("WiFi timeout");
      return false;
    }
    delay(250);
  }
  Serial.print("WiFi connected; IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool ensureClock(uint32_t timeoutMs = 20000) {
  // Certificate validity checks require a reasonably accurate system clock.
  if (time(nullptr) > 1700000000) return true;
  static bool ntpRequested = false;
  if (!ntpRequested) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpRequested = true;
  }
  const uint32_t start = millis();
  while (time(nullptr) <= 1700000000) {
    if (millis() - start >= timeoutMs) {
      Serial.println("NTP time not available; skipping VPS TLS connection for now");
      return false;
    }
    delay(250);
  }
  Serial.println("System time synchronized for TLS certificate validation");
  return true;
}

/* ---------------- Adafruit MQTT ---------------- */
bool ensureAdafruitMqtt(uint32_t timeoutMs = 10000) {
  if (aioMqtt.connected()) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
  const uint32_t start = millis();
  while (!aioMqtt.connected()) {
    Serial.print("Adafruit MQTT connecting...");
    if (aioMqtt.connect() == 0) {
      Serial.println("connected");
      return true;
    }
    Serial.println("failed");
    aioMqtt.disconnect();
    if (millis() - start > timeoutMs) {
      Serial.println("Adafruit MQTT timeout");
      return false;
    }
    delay(1000);
  }
  return true;
}

/* ---------------- VPS MQTT over verified TLS ---------------- */
void initVpsMqtt() {
#if defined(ESP8266)
  vpsTlsClient.setTrustAnchors(&vpsTrustAnchor);
#else
  vpsTlsClient.setCACert(VPS_MQTT_CA_CERT);
#endif
  vpsTlsClient.setTimeout(10000);
  IPAddress vpsBrokerIp;

  if (!vpsBrokerIp.fromString(VPS_MQTT_HOST)) {
    Serial.println("ERROR: VPS_MQTT_HOST must be an IPv4 address.");
  } else {
    vpsMqtt.setServer(vpsBrokerIp, VPS_MQTT_PORT);
  }
  vpsMqtt.setBufferSize(512);
  vpsMqtt.setSocketTimeout(8);
#if defined(ESP8266)
  snprintf(vpsClientId, sizeof(vpsClientId), "shlogger-%06X", ESP.getChipId());
#elif defined(ESP32)
  snprintf(vpsClientId, sizeof(vpsClientId), "shlogger-%08X",
           static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFFFULL));
#endif
}

bool ensureVpsMqtt(uint32_t timeoutMs = 10000) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (vpsMqtt.connected()) return true;
  if (!ensureClock()) return false;

  const uint32_t start = millis();
  while (!vpsMqtt.connected()) {
    Serial.printf("VPS MQTT TLS connecting to %s:%u...\n",
                  VPS_MQTT_HOST, static_cast<unsigned>(VPS_MQTT_PORT));
    if (vpsMqtt.connect(vpsClientId, VPS_MQTT_USERNAME, VPS_MQTT_PASSWORD)) {
      Serial.println("VPS MQTT TLS connected");
      return true;
    }
    Serial.printf("VPS MQTT connect failed (state %d)\n", vpsMqtt.state());
    #if defined(ESP8266)
    char tlsError[160] = {0};
    int tlsCode = vpsTlsClient.getLastSSLError( tlsError, sizeof(tlsError));
    Serial.printf("BearSSL error %d: %s\n", tlsCode, tlsError);
    Serial.printf("Free heap: %u bytes; largest block: %u bytes\n",
              ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
    #endif

    Serial.printf("System time: %lu\n",
              static_cast<unsigned long>(time(nullptr)));

    vpsTlsClient.stop();
    if (millis() - start > timeoutMs) {
      Serial.println("VPS MQTT timeout; will retry later");
      return false;
    }
    delay(1000);
  }
  return true;
}

/*
 * Parse both legacy queue rows and new rows with per-destination state:
 * millis,count,temp1,...,tempN,rssi,adafruitDone,vpsDone
 * Legacy rows are treated as pending at both destinations.
 */
bool parseStoredLine(String line, uint8_t &count, float temps[], int &rssi,
                     bool &adafruitDone, bool &vpsDone, String &baseLine) {
  line.trim();
  int pos = 0;
  int comma = line.indexOf(',', pos); // end of millis
  if (comma < 0) return false;
  pos = comma + 1;
  comma = line.indexOf(',', pos); // end of count
  if (comma < 0) return false;
  const long parsedCount = line.substring(pos, comma).toInt();
  if (parsedCount <= 0 || parsedCount > MAX_SENSORS) return false;
  count = static_cast<uint8_t>(parsedCount);
  pos = comma + 1;

  for (uint8_t i = 0; i < count; i++) {
    comma = line.indexOf(',', pos);
    if (comma < 0) return false;
    temps[i] = line.substring(pos, comma).toFloat();
    pos = comma + 1;
  }

  const int rssiEnd = line.indexOf(',', pos);
  if (rssiEnd < 0) {
    // Original queue format, no delivery flags yet.
    rssi = line.substring(pos).toInt();
    baseLine = line;
    adafruitDone = false;
    vpsDone = false;
    return true;
  }

  rssi = line.substring(pos, rssiEnd).toInt();
  baseLine = line.substring(0, rssiEnd);
  const int flagEnd = line.indexOf(',', rssiEnd + 1);
  if (flagEnd < 0) {
    adafruitDone = line.substring(rssiEnd + 1).toInt() != 0;
    vpsDone = false;
    return true;
  }
  adafruitDone = line.substring(rssiEnd + 1, flagEnd).toInt() != 0;
  vpsDone = line.substring(flagEnd + 1).toInt() != 0;
  return true;
}

bool publishMeasurementToAdafruit(uint8_t count, float temps[]) {
  if (!ensureAdafruitMqtt()) return false;
  for (uint8_t i = 0; i < count; i++) {
    Serial.printf("Adafruit sensor %u: %.2f C\n",
                  static_cast<unsigned>(i + 1), temps[i]);
    if (!publishToAdafruitFeed(i, temps[i])) {
      Serial.printf("Adafruit publish failed for sensor %u\n",
                    static_cast<unsigned>(i + 1));
      return false;
    }
    aioMqtt.processPackets(10);
    delay(50);
  }
  aioMqtt.ping();
  return true;
}

bool publishMeasurementToVps(uint8_t count, float temps[]) {
  if (!ensureVpsMqtt()) return false;
  for (uint8_t i = 0; i < count; i++) {
    Serial.printf("VPS sensor %u: %.2f C\n",
                  static_cast<unsigned>(i + 1), temps[i]);
    if (!publishToVps(i, temps[i])) return false;
    vpsMqtt.loop();
    delay(30);
  }
  return true;
}

/* ---------------- Upload queue ---------------- */
void publishStoredData() {
  if (!ensureWiFi()) return;
  File src = LittleFS.open(DATA_FILE, "r");
  if (!src) return;
  File tmp = LittleFS.open("/tmp.csv", "w");
  if (!tmp) {
    src.close();
    Serial.println("Could not create temporary queue file; retaining backlog");
    return;
  }

  while (src.available()) {
    String line = src.readStringUntil('\n');
    if (line.length() < 5) continue;

    uint8_t count = 0;
    float temps[MAX_SENSORS] = {0};
    int rssi = 0;
    bool adafruitDone = false;
    bool vpsDone = false;
    String baseLine;
    if (!parseStoredLine(line, count, temps, rssi,
                         adafruitDone, vpsDone, baseLine)) {
      Serial.println("Skipping malformed measurement row in queue");
      continue;
    }

    Serial.printf("Queue row: %u sensors, RSSI %d, Adafruit=%s, VPS=%s\n",
                  static_cast<unsigned>(count), rssi,
                  adafruitDone ? "done" : "pending",
                  vpsDone ? "done" : "pending");

    // Each destination has its own persisted completion flag, so an outage
    // at one destination does not hold up the other or duplicate its backlog.
    if (!adafruitDone) {
      if (publishMeasurementToAdafruit(count, temps)) {
        adafruitDone = true;
      }
    }
    if (!vpsDone) {
      if (publishMeasurementToVps(count, temps)) {
        vpsDone = true;
      }
    }

    // Keep only rows that have not been accepted by both destinations.
    if (!(adafruitDone && vpsDone)) {
      tmp.printf("%s,%d,%d\n", baseLine.c_str(),
                 adafruitDone ? 1 : 0, vpsDone ? 1 : 0);
    }
  }

  src.close();
  tmp.close();
  LittleFS.remove(DATA_FILE);
  if (!LittleFS.rename("/tmp.csv", DATA_FILE)) {
    Serial.println("WARNING: Could not replace queue file with pending rows");
  }
}

/* ---------------- Sensor discovery ---------------- */
void discoverSensors() {
  sensorCount = 0;

  for (uint8_t attempt = 1; attempt <= 5; attempt++) {
    sensors.begin();
    const uint8_t detected = sensors.getDeviceCount();

    Serial.printf(
        "DS18B20 scan %u/5: found %u sensor(s)\n",
        static_cast<unsigned>(attempt),
        static_cast<unsigned>(detected)
    );

    if (detected > 0) {
      sensorCount = detected;
      break;
    }

    delay(1000);
  }

  if (sensorCount > MAX_SENSORS) {
    sensorCount = MAX_SENSORS;
  }

  if (sensorCount == 0) {
    Serial.println(
        "ERROR: No DS18B20 sensors detected after 5 scans"
    );
    return;
  }

  for (uint8_t i = 0; i < sensorCount; i++) {
    if (sensors.getAddress(sensorAddresses[i], i)) {
      sensors.setResolution(sensorAddresses[i], 12);

      Serial.printf("  Sensor %u: ",
                    static_cast<unsigned>(i + 1));
      printAddress(sensorAddresses[i]);
      Serial.println();
    } else {
      Serial.printf("ERROR: Could not read address of sensor %u\n",
                    static_cast<unsigned>(i + 1));
    }
  }
}

/* ---------------- Setup and main loop ---------------- */
void setup() {
  Serial.begin(115200);
  delay(1000);
#if defined(ESP32)
  Serial.println("\nESP32 DS18B20 Logger — Adafruit + VPS MQTT/TLS");
#else
  Serial.println("\nESP8266 DS18B20 Logger — Adafruit + VPS MQTT/TLS");
#endif

  initFS();
  initVpsMqtt();
  ensureWiFi();
  discoverSensors();
  Serial.println("Setup complete");
}

/* ---------------- Loop ---------------- */

void loop() {
  const unsigned long now = millis();
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
        if (temperatures[i] == DEVICE_DISCONNECTED_C) allValid = false;
        if (i > 0) Serial.print(" / ");
        Serial.printf("%.2f", temperatures[i]);
      }
      Serial.println(" °C");

      // FR-M-0007: Skip first measurement after boot (initial reading is unreliable)
      if (!firstMeasurementDone) {
        Serial.println("(First measurement skipped - FR-M-0007)");
        firstMeasurementDone = true;
      } else if (allValid) {
        storeToFS(temperatures, sensorCount);
      } else {
        Serial.println("Sensor error — some readings invalid");
      }
    }
  }

  if (hasStoredData()) publishStoredData();
  if (aioMqtt.connected()) aioMqtt.processPackets(10);
  if (aioMqtt.connected()) aioMqtt.ping();
  if (vpsMqtt.connected()) vpsMqtt.loop();
  delay(100);
}
