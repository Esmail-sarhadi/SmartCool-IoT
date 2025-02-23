# SmartCool-IoT 🌊❄️

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Overview 📝
An ESP32-based smart evaporative cooler controller with touch control and MQTT capabilities. This project provides intelligent control of evaporative coolers with advanced features like temperature control, scheduling, and remote management.

## Key Features ⭐

- 🎯 Advanced Touch Control:
  - Power On/Off button
  - Water pump control
  - Speed adjustment (Low/High)
  - Touch lock capability
- 🌡️ Smart Temperature Control:
  - DHT11 temperature and humidity sensor
  - Target temperature setting
  - Automatic speed adjustment based on temperature
- ⏰ Scheduling Features:
  - Set On/Off times
  - Weekly programming
  - Iran timezone support
- 🌐 Connectivity:
  - MQTT support
  - Configurable WiFi connection
  - Status publishing and command reception

## Hardware Requirements 🛠️

- ESP32 Development Board
- DHT11 Temperature & Humidity Sensor
- 3 Relays for controlling:
  - High speed
  - Low speed
  - Water pump
- 4 Touch buttons

## Pin Configuration 📌

```
RELAY_HIGH    -> GPIO25 (High speed relay)
RELAY_LOW     -> GPIO26 (Low speed relay)
RELAY_PUMP    -> GPIO27 (Water pump relay)

TOUCH_POWER   -> GPIO32 (Power button)
TOUCH_PUMP    -> GPIO18 (Pump control button)
TOUCH_SPEED   -> GPIO19 (Speed control button)
TOUCH_MODE    -> GPIO21 (Mode control button)

DHT_PIN       -> GPIO4  (Temperature sensor)
```

## Required Libraries 📚

- WiFiManager
- PubSubClient
- DHT sensor library
- ArduinoJson
- TimeLib
- Timezone

## Installation & Setup 🔧

1. Clone the repository:
   ```bash
   git clone https://github.com/Esmail-sarhadi/SmartCool-IoT.git
   ```

2. Install required libraries through Arduino IDE Library Manager

3. Upload the code to your ESP32

4. WiFi Setup:
   - On first boot, device creates an Access Point named "Cooler-Setup"
   - Connect to this network and configure WiFi settings

## MQTT Configuration 🔐

Default MQTT settings:
- Broker: broker.hivemq.com
- Port: 1883
- Topics:
  - cooler/temperature: Current temperature
  - cooler/status: Device status
  - cooler/control: Control commands
  - cooler/schedule: Schedule settings
  - cooler/time: Time settings

## Features in Detail 🎯

### Touch Control System
- Long press (5 seconds) on power button to lock/unlock touch controls
- Touch feedback with debounce protection
- Independent control for pump and fan speeds

### Temperature Control
- Automatic speed adjustment based on target temperature
- Temperature monitoring and reporting
- Customizable temperature thresholds

### Scheduling System
- Daily and weekly scheduling options
- Timezone-aware scheduling (Iran timezone)
- Persistent schedule storage

### MQTT Integration
- Real-time status updates
- Remote control capability
- JSON-based communication
- Automatic reconnection handling

## Error Handling 🛡️

- Robust WiFi connection management
- MQTT connection recovery
- Sensor reading validation
- Touch input debouncing

## Contributing 🤝

Contributions are welcome! Please feel free to submit a Pull Request.

## Author ✍️

**Esmail Sarhadi**
- GitHub: [@Esmail-sarhadi](https://github.com/Esmail-sarhadi)

## License 📄

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support 💬

If you have any questions or run into issues, please open an issue in the GitHub repository.

---
⭐ Don't forget to star this repo if you find it helpful!
