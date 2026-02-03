# Functional Specification Document (FSD)

## SmartHome Data Logger

## Version

V1.2

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
The system shall support 0 to 10 DS18B20 sensor connected.

**FR-M-0002**
The system shall measure temperature in °C from each DS18B20 sensor connected.

**FR-M-0003**
All measurements shall be performend in an interval of **60 seconds**

## 3.2. Sending data to IoT backend
### 3.2.1. Adafruit IoT backen
**FR-AIoTB-0001**
The system shall send data to the Adafruit IoT backend after each measurement.

## 3.3. Diagnostics

**FR-D-0001**
The system shall log the WiFi signal strength (RSSI) with each measurement for diagnostic purposes.

**FR-D-0002**
The stored measurement record shall include: timestamp, temperature values, and RSSI value.

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
