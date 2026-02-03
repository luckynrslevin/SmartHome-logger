# SmartHome-logger

Temperature logger with DS18B20 sensors and Adafruit IO integration.

Supports both ESP8266 and ESP32 boards.

## Features

- Dual DS18B20 temperature sensor support
- Publishes to Adafruit IO via MQTT
- Offline-resilient: stores measurements to LittleFS when WiFi is unavailable
- Automatic backlog upload when connectivity is restored
- RSSI logging for WiFi diagnostics
- Multi-board support (ESP8266 and ESP32)

## Supported Boards

| Board | PlatformIO env | Data Pin |
|-------|----------------|----------|
| ESP8266 NodeMCU v2 | `esp8266` | D4 |
| Sparkfun ESP32 Thing Plus | `esp32` | GPIO4 |

## Hardware

- ESP8266 (NodeMCU, Wemos D1 Mini) **or** ESP32 (Sparkfun Thing Plus)
- 2x DS18B20 temperature sensors
- 4.7kΩ pull-up resistor on the data line

### Wiring - ESP8266

```
                        4.7kΩ
                      ┌─/\/\/─┐
                      │       │
┌─────┐  ┌──────────┐ │       │┌──────────────┐                       ┌──────────────┐
│     │  │  ESP8266 │ │       ││   DS18B20    │                       │   DS18B20    │
│ USB │  │          │ │       ││   (Sensor 1) │                       │   (Sensor 2) │
│  5V ├──┤ VIN      │ │       ││              │                       │              │
│     │  │     3.3V ├─┴───────┼┤ VCC      ────┼───────────────────────┤ VCC          │
│ GND ├──┤ GND      │         ││              │                       │              │
│     │  │       D4 ├─────────┴┤ DATA     ────┼───────────────────────┤ DATA         │
│     │  │          │          │              │                       │              │
└─────┘  │      GND ├──────────┤ GND      ────┼───────────────────────┤ GND          │
         │          │          │              │                       │              │
         └──────────┘          └──────────────┘                       └──────────────┘
```

| DS18B20 | ESP8266 |
|---------|---------|
| VCC     | 3.3V    |
| GND     | GND     |
| DATA    | D4      |

### Wiring - ESP32 (Sparkfun Thing Plus)

```
                         4.7kΩ
                       ┌─/\/\/─┐
                       │       │
┌─────┐  ┌───────────┐ │       │┌──────────────┐                       ┌──────────────┐
│     │  │   ESP32   │ │       ││   DS18B20    │                       │   DS18B20    │
│ USB │  │ Thing Plus│ │       ││   (Sensor 1) │                       │   (Sensor 2) │
│  5V ├──┤ VUSB      │ │       ││              │                       │              │
│     │  │      3.3V ├─┴───────┼┤ VCC      ────┼───────────────────────┤ VCC          │
│ GND ├──┤ GND       │         ││              │                       │              │
│     │  │      GPIO4├─────────┴┤ DATA     ────┼───────────────────────┤ DATA         │
│     │  │           │          │              │                       │              │
└─────┘  │       GND ├──────────┤ GND      ────┼───────────────────────┤ GND          │
         │           │          │              │                       │              │
         └───────────┘          └──────────────┘                       └──────────────┘
```

| DS18B20 | ESP32 Thing Plus |
|---------|------------------|
| VCC     | 3.3V             |
| GND     | GND              |
| DATA    | GPIO4            |

## Quick Start

### 1. Install PlatformIO

```bash
brew install platformio
```

Or via pip:

```bash
pip install platformio
```

### 2. Clone and configure

```bash
git clone <repo-url>
cd SmartHome-logger
cp src/config.h.example src/config.h
```

Edit `src/config.h` with your credentials:

```c
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
#define AIO_USERNAME    "your_adafruit_username"
#define AIO_KEY         "your_adafruit_key"
```

### 3. Build and flash

Connect your board via USB, then:

```bash
# ESP8266
pio run -e esp8266 -t upload

# ESP32
pio run -e esp32 -t upload
```

### 4. Monitor serial output

```bash
pio device monitor
```

## Commands

| Command | Description |
|---------|-------------|
| `pio run` | Build all environments |
| `pio run -e esp8266` | Build for ESP8266 only |
| `pio run -e esp32` | Build for ESP32 only |
| `pio run -e esp8266 -t upload` | Build and flash ESP8266 |
| `pio run -e esp32 -t upload` | Build and flash ESP32 |
| `pio device monitor` | Serial monitor |
| `pio run -t clean` | Clean build files |

## Configuration

### Adafruit IO

Create two feeds in your Adafruit IO account:
- `temp_sensor_1`
- `temp_sensor_2`

Get your API key from https://io.adafruit.com → My Key

### Sensor Addresses

Find your DS18B20 addresses using a scanner sketch, then update `sensorAddresses[]` in `src/main.cpp`.

### Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `PUBLISH_INTERVAL` | 60000 ms | Measurement interval |
| `ONE_WIRE_BUS` | D4 (ESP8266) / GPIO4 (ESP32) | DS18B20 data pin |
| `SENSOR_COUNT` | 2 | Number of sensors |

## Project Structure

```
SmartHome-logger/
├── platformio.ini      # Build configuration (ESP8266 + ESP32)
├── src/
│   ├── main.cpp        # Main firmware (multi-board)
│   ├── config.h        # Your credentials (gitignored)
│   └── config.h.example
└── README.md
```

## Data Format

Measurements are stored locally in `/temps.csv` with the format:

```
millis,temp1,temp2,rssi
```

## License

MIT
