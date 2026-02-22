# GestureRecog (ESP-GloveStuff) 🧤

**Low-latency gesture recognition and data capture for ESP32-based smart gloves.**

GestureRecog is an embedded system project built on the ESP-IDF framework. It is designed to interface with multiple sensors (typically flex sensors or IMUs) on a glove to capture, process, and transmit finger movement data in real-time.

## ✨ Features

- **Multi-Channel ADC:** Efficiently samples 5+ analog channels simultaneously for finger tracking.
- **Real-time Calibration:** Dynamic min/max value tracking to account for sensor drift and user hand size.
- **Auto-Mapping:** Automatically maps raw sensor voltages to a normalized 12-bit range (0-4096) for consistent processing.
- **Disable Logic:** Built-in threshold detection to identify when the device is idle or disconnected.
- **Low Latency:** Optimized for FreeRTOS with a 100ms polling cycle (adjustable).

## 🛠️ Technical Specifications

- **Target Hardware:** ESP32 / ESP32-S3 / ESP32-C3.
- **Framework:** [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/) (C-based).
- **Communication:** Serial output for data visualization and debugging.
- **Build System:** CMake.

## 🚀 Getting Started

### Prerequisites
- [ESP-IDF v5.0+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) installed and configured.

### Build & Flash

1. Clone the repository:
   ```bash
   git clone https://github.com/XotEmBotZ/gestureRecog.git
   cd gestureRecog
   ```

2. Set the target (e.g., esp32):
   ```bash
   idf.py set-target esp32
   ```

3. Build the project:
   ```bash
   idf.py build
   ```

4. Flash and monitor:
   ```bash
   idf.py flash monitor
   ```

## 📈 Data Output Format
The system outputs data in a structured format via UART:
- `>idx:mapped_value`: Normalized sensor data.
- `>ridx:raw_value`: Raw ADC readings.
- `>d:is_disabled`: Binary state of the sensor array.
