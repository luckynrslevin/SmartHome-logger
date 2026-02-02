# SmartHome-logger

ESP8266-based temperature logger with DS18B20 sensors and Adafruit IO integration.

## Features

- Dual DS18B20 temperature sensor support
- Publishes to Adafruit IO via MQTT
- Offline-resilient: stores measurements to LittleFS when WiFi is unavailable
- Automatic backlog upload when connectivity is restored
- RSSI logging for WiFi diagnostics
- Watchdog-safe implementation

## Hardware

- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- 2x DS18B20 temperature sensors
- 4.7kΩ pull-up resistor on the data line

### Wiring

| DS18B20 | ESP8266 |
|---------|---------|
| VCC     | 3.3V    |
| GND     | GND     |
| DATA    | D4      |

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

Connect your ESP8266 via USB, then:

```bash
pio run -t upload
```

### 4. Monitor serial output

```bash
pio device monitor
```

## Commands

| Command | Description |
|---------|-------------|
| `pio run` | Build firmware |
| `pio run -t upload` | Build and flash |
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
| `ONE_WIRE_BUS` | D4 | DS18B20 data pin |
| `SENSOR_COUNT` | 2 | Number of sensors |

## Project Structure

```
SmartHome-logger/
├── platformio.ini      # Build configuration
├── src/
│   ├── main.cpp        # Main firmware
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
