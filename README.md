# SmartHome-logger

Temperature logger with DS18B20 sensors and Adafruit IO integration.

Supports both ESP8266 and ESP32 boards.

## Features

- **Dynamic DS18B20 sensor detection** - Supports 0-10 sensors, auto-discovered at startup
- Publishes to Adafruit IO via MQTT
- Offline-resilient: stores measurements to LittleFS when WiFi is unavailable
- Automatic backlog upload when connectivity is restored
- RSSI logging for WiFi diagnostics
- Multi-board support (ESP8266 and ESP32)
- **OTA updates** - Update firmware over WiFi without USB connection

## Supported Boards

| Board | PlatformIO env | Data Pin |
|-------|----------------|----------|
| ESP8266 NodeMCU v2 | `esp8266` | D4 |
| Sparkfun ESP32 Thing Plus | `esp32` | GPIO4 |

## Hardware

- ESP8266 (NodeMCU, Wemos D1 Mini) **or** ESP32 (Sparkfun Thing Plus)
- 0-10 DS18B20 temperature sensors (auto-detected)
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
#define OTA_PASSWORD    "your_ota_password"
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

Create feeds in your Adafruit IO account for each sensor:
- `temp_sensor_1`
- `temp_sensor_2`
- ... up to `temp_sensor_10`

Get your API key from https://io.adafruit.com → My Key

### Sensor Detection

Sensors are automatically discovered at startup. The serial output shows each sensor's address:

```
DS18B20 sensors found: 3
  Sensor 1: 28E11732000000062
  Sensor 2: 288FBF31000000037
  Sensor 3: 28AA12340000000FF
```

### Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `PUBLISH_INTERVAL` | 60000 ms | Measurement interval |
| `ONE_WIRE_BUS` | D4 (ESP8266) / GPIO4 (ESP32) | DS18B20 data pin |
| `MAX_SENSORS` | 10 | Maximum sensors supported |

## OTA Updates

The firmware supports over-the-air updates via WiFi. No USB connection required after initial flash.

### Configuration

Set your OTA password in `src/config.h`:

```c
#define OTA_PASSWORD    "your_secure_password"
```

### Upload via OTA

**Note:** First flash must be via USB. After that, OTA updates work wirelessly.

```bash
# ESP8266
pio run -e esp8266 -t upload --upload-port smarthome-esp8266.local -a YOUR_OTA_PASSWORD

# ESP32
pio run -e esp32 -t upload --upload-port smarthome-esp32.local -a YOUR_OTA_PASSWORD
```

Or specify the IP address directly:

```bash
pio run -e esp32 -t upload --upload-port 192.168.1.100 -a YOUR_OTA_PASSWORD
```

### Serial Output

During OTA update, the serial console shows progress:

```
OTA Start: firmware
OTA Progress: 50%
OTA Progress: 100%
OTA End - Rebooting...
```

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
millis,count,temp1,temp2,...,tempN,rssi
```

Example with 3 sensors:
```
123456,3,21.50,22.75,19.25,-65
```

## License

MIT
