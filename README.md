# Pump Timer RTOS

A multi-channel relay timer system built on the Arduino Nano (ATmega328P) using FreeRTOS for task management. This project provides precise scheduling for up to 4 relays, making it ideal for irrigation, hydroponics, or automated pump control.

## 🚀 Features

- **FreeRTOS Integration:** Uses a real-time operating system for reliable task scheduling and relay control.
- **4-Channel Control:** Individually configurable relays (connected to Digital Pins 9-12).
- **Dual Scheduling Modes:**
  - **Fixed Time:** Trigger a relay at a specific time of day for a set duration.
  - **Interval:** Trigger a relay repeatedly based on a defined interval (e.g., every 4 hours).
- **Manual Override:** Latch relays on or off manually via the serial interface or web dashboard.
- **Persistent Storage:** All configurations are saved to EEPROM, ensuring settings survive power cycles.
- **RTC Support:** Uses the DS3231 High-Precision RTC for accurate timekeeping.
- **Web Dashboard:** Modern, browser-based configurator using the Web Serial API (no additional server required).

## 🛠 Hardware Requirements

- **Microcontroller:** Arduino Nano (ATmega328P)
- **RTC:** DS3231 I2C Real-Time Clock
- **Relays:** 4-Channel Relay Module (Active HIGH)
- **Connections:**
  - Relay 1: D12
  - Relay 2: D11
  - Relay 3: D10
  - Relay 4: D9
  - RTC: A4 (SDA), A5 (SCL)

## 📂 Project Structure

- `src/main.cpp`: Core logic, FreeRTOS tasks, and serial command handler.
- `webconfigurator/index.html`: Web-based configuration tool.
- `platformio.ini`: PlatformIO project configuration.

## ⚙️ Software Architecture

The system runs two primary FreeRTOS tasks:

1. **`TaskRelayControl` (Priority 2):** Monitors the RTC and compares current time against schedules to toggle relays.
2. **`TaskSerialMonitor` (Priority 1):** Handles incoming serial commands and broadcasts status updates in a JSON-like format.

A **Semaphore Mutex** (`configMutex`) is used to ensure thread-safe access to the relay configuration shared between the control and monitor tasks.

## 💻 Configuration

### Web Configurator
The included `webconfigurator/index.html` allows you to configure the system visually.
1. Open `webconfigurator/index.html` in a modern browser (Chrome or Edge recommended).
2. Click **Connect to Nano** and select the appropriate COM port.
3. Configure schedules or toggle relays manually.

### Serial Commands
The system accepts raw serial commands at **9600 Baud**:

| Command | Description | Example |
| :--- | :--- | :--- |
| `S` | Request current status (JSON) | `S` |
| `C` | Factory Reset / Clear EEPROM | `C` |
| `M <id>` | Manual Toggle (Latch) | `M 1` |
| `F <id> <H> <M> <Dur>` | Set Fixed Schedule (Dur in mins) | `F 1 09 30 15` (Relay 1 at 9:30 for 15m) |
| `I <id> <Int> <Dur>` | Set Interval (Int/Dur in secs) | `I 2 3600 60` (Relay 2 every 1h for 1m) |

## 📦 Installation

1. Install [PlatformIO](https://platformio.org/).
2. Clone this repository.
3. Connect your Arduino Nano.
4. Run the following command to build and upload:
   ```bash
   pio run --target upload
   ```

## 📜 License
This project is open-source and available under the MIT License.
