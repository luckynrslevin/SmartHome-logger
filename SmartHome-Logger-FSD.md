# Functional Specification Document (FSD)

## SmartHome Data Logger

## Version

V1.4

## Date
2026-02-03

------

## 1. Purpose and Scope
This document specifies the functional requirements, behavior, interfaces and constraints of a SmartHome data logging device based on ESP8266 and ESP32 microcontrollers.

## 2. System overview
### 2.1. General
- ESP8266 or ESP32 microcontroller.
- Tested on following MCUs:
  - Lolin D1 mini pro (ESP8266)
  - SparkFun ESP32 Thing Plus
- Development environment: PlatformIO CLI
- Temperature sensors: Up to 10 times DS18B20
- Temperature, Humidity and air pressure sensor: BME280 (new feature)
- Grundfoss Alpha Go 2 pumps vi BLE interface using https://github.com/parameter-pollution/esphome-grundfos-alpha2-go/ (new feature) - only for ESP32, since it requires BLE.
- Supported IoT backends:
  - Publich cloud: Adafruit IoT cloud
  - Private cloud: DIY IoT backend based on telegraf, mosquitto, influxdb, grafana (new feature)

### 2.2. ESP8266 (Lolin D1 pro mini)

```
                        4.7kΩ
                      ┌─/\/\/─┐
                      │       │
┌─────┐  ┌──────────┐ │       │┌──────────┐   ┌────────────┐   ┌──────────┐
│     │  │  ESP8266 │ │       ││  DS18B20 │   │  DS18B20   │   │  DS18B20 │
│ USB │  │          │ │       ││  (Nr. 1) │   │  (Nr. ...) │   | (Nr. 10) |
│  5V ├──┤ VIN      │ │       ││          │   │            │   |          |
│     │  │     3.3V ├─┴───────┼┤ VCC   ───┼───┤ VCC     ───┼───┼── VCC    |
│ GND ├──┤ GND      │    °C   ││          │   │            │   |          |
│     │  │       D4 ├─────────┴┤ DATA  ───┼───┤ DATA    ───┼───┼── DATA   |
│     │  │          │          │          │   │            │   |          |
└─────┘  │      GND ├──────────┤ GND   ───┼───┤ GND     ───┼───┼── GND    |
         │          │          │          │   │            │   |          |
         └──────────┘          └──────────┘   └────────────┘   └──────────┘
```

### 2.3. ESP2 (SparkFun ESP32 Thing Plus)

```
                                                                               ┌----------------┐
                                                                               | Grundfos Alpha |
                                                                               │   Go 2 Pumpe   │
                                                                               |  ┌──────────┐  |
                                                                               │  │   BLE    │  │
                                                                               |  │  Beacon  │  |
                                                                               │  └──────────┘  │
                                                                               └────────────────┘
                        4.7kΩ                                                       ))) │
                      ┌─/\/\/─┐                                                 )))     │
                      │       │                                             )))         │
┌─────┐  ┌───────────┐│       │┌──────────┐   ┌────────────┐   ┌──────────┐             │
│     │  │   ESP32   ││       ││  DS18B20 │   │  DS18B20   │   │  DS18B20 │             │
│ USB │  │ Thing Plus││       ││  (Nr. 1) │   │  (Nr. ...) │   │ (Nr. 10) │             │
│  5V ├──┤ VUSB      ││       ││          │   │            │   │          │             │
│     │  │      3.3V ├┴───────┼┤ VCC   ───┼───┤ VCC     ───┼───┤── VCC    │             │
│ GND ├──┤ GND       │   °C   ││          │   │            │   │          │  BLE Advertisement
│     │  │      GPIO4├────────┴┤ DATA  ───┼───┤ DATA    ───┼───┤── DATA   │   • Durchfluss
│     │  │           │         │          │   │            │   │          │   • Förderhöhe
└─────┘  │       GND ├─────────┤ GND   ───┼───┤ GND     ───┼───┤── GND    │   • Leistung
         │           │         │          │   │            │   │          │   • Temperatur
         │  ┌──────┐ │         └──────────┘   └────────────┘   └──────────┘   • Betriebsstd.
         │  │ BLE  │◄├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
         │  │Radio │ │                         (kabellose Verbindung)
         │  └──────┘ │
         └───────────┘
```

# 3. Functional requirements

## 3.1. Measurement functionality

**FR-M-0001**
The system shall automatically detect and support 0 to 10 DS18B20 sensors connected to the 1-Wire bus.

**FR-M-0002**
The system shall measure temperature in °C from each DS18B20 sensor connected.

**FR-M-0003**
All measurements shall be performed in an interval of **60 seconds**.

**FR-M-0004**
The system shall discover all connected DS18B20 sensors at startup by scanning the 1-Wire bus.

**FR-M-0005**
The system shall store the unique 64-bit address of each discovered sensor.

**FR-M-0006**
The system shall set 12-bit resolution for all discovered sensors.

**FR-M-0007**
After boot the initial measurement of the temperature shows a wrong value, therefore the system shall omit the first temperature measurement after boot.

## 3.2. Sending data to IoT backend

### 3.2.1. Adafruit IoT backend

**FR-AIoTB-0001**
The system shall send data to the Adafruit IoT backend after each measurement.

**FR-AIoTB-0002**
The system shall publish each sensor's temperature to a separate MQTT feed named `temp_sensor_N` where N is the sensor index (1-10).

**FR-AIoTB-0003**
The system shall only publish data for sensors that were discovered at startup.

## 3.3. Diagnostics

**FR-D-0001**
The system shall log the WiFi signal strength (RSSI) with each measurement for diagnostic purposes.

**FR-D-0002**
The stored measurement record shall include: timestamp, sensor count, temperature values, and RSSI value.

**FR-D-0003**
At startup, the system shall log the number of DS18B20 sensors discovered.

**FR-D-0004**
At startup, the system shall log the 64-bit address of each discovered sensor.

**FR-D-0005**
The system shall log "No sensors connected" if no DS18B20 sensors are discovered.

**FR-D-0006**
The system shall log each measurement cycle with temperature values from all sensors.

**FR-D-0007**
The system shall log the firmware version at startup.

**FR-D-0008**
The firmware version shall be defined as a constant in the source code.

## 3.4. Data Storage

**FR-DS-0001**
Measurements shall be stored in CSV format with fields: timestamp, sensor count, temperature values (one per sensor), and RSSI.

**FR-DS-0002**
The storage format shall be: `millis,count,temp1,temp2,...,tempN,rssi`

**FR-DS-0003**
The system shall only store measurements when all sensor readings are valid.

#  4. Non functional requirements

## 4.1. WIFI Connectivity

**NFR-WIFI-0001**
The system shall be resillient to unstable wifi connectivity.

**NFR-WIFI-0002**
The system shall temporarily store all measurements, which were not successfully send to the backend.

**NFR-WIFI-0003**
In case the MCU offers a persistent data store, it shall be used for the temporary storage of measurements (NFR-WIFI-0002).

**NFR-WIFI-0004**
In case not WIFI connection is available, the system shall reestablish wifi connection regularily before sending data to the backend.

## 4.2. Bluetooth Low Energy Connectivity

(Reserved for future BLE requirements)

## 4.3. Security

**NFR-SEC-0001**
Sensitive configuration data (WiFi credentials, API keys) shall be stored in a separate configuration file.

**NFR-SEC-0002**
The configuration file containing secrets shall be excluded from version control via `.gitignore`.

**NFR-SEC-0003**
A template configuration file (`config.h.example`) shall be provided with placeholder values.

## 4.4. Build System

**NFR-BUILD-0001**
The system shall be buildable via PlatformIO CLI without requiring an IDE.

**NFR-BUILD-0002**
The build configuration shall support multiple target boards from a single codebase.

**NFR-BUILD-0003**
Flashing the MCU shall be possible with a single command: `pio run -e <environment> -t upload`

## 4.5. Documentation

**NFR-DOC-0001**
The README shall include wiring diagrams for each supported board.

**NFR-DOC-0002**
The README shall include step-by-step setup instructions for cloning, configuring, and flashing.

## 4.6. Over-the-Air Updates

**NFR-OTA-0001**
The system shall support firmware updates over WiFi without requiring physical USB connection.

**NFR-OTA-0002**
The system shall use the ArduinoOTA protocol for over-the-air updates.

**NFR-OTA-0003**
OTA updates shall be protected with a configurable password stored in the configuration file.

**NFR-OTA-0004**
The system shall log OTA update progress and status to the serial console.

**NFR-OTA-0005**
The system shall continue normal measurement operations while listening for OTA update requests.

**NFR-OTA-0006**
The system shall automatically reboot after a successful OTA update.
