# GestureRecog (ESP-GloveStuff) 🧤

**Low-latency on-device gesture recognition and data capture using ESP32-S3 and k-NN.**

GestureRecog is an embedded system designed to recognize hand gestures using flex sensors. It features on-device training, data storage, and real-time inference, all while maintaining extreme portability and low power consumption.

---

## 🛠️ Tech Stack

### Firmware (ESP32-S3)
- **Framework:** [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/) (C-based).
- **DSP Engine:** [ESP-DSP](https://github.com/espressif/esp-dsp) for SIMD-accelerated k-Nearest Neighbors (k-NN) inference.
- **Storage:** SPIFFS (Serial Peripheral Interface Flash File System) for persistent on-device dataset storage.
- **OS:** FreeRTOS for multi-tasking and deterministic sampling.

### Frontend (Web Dashboard)
- **Framework:** Next.js 15 (React 19).
- **Styling:** Tailwind CSS 4.0.
- **Visualization:** Chart.js with `react-chartjs-2` for real-time sensor telemetry.
- **Connectivity:** Web Serial API (via `web-serial-polyfill`) for browser-to-hardware communication (supports Android/Chrome).

---

## 📌 Pin Configuration (ESP32-S3)

The project uses the internal ADC1 unit. The default mapping for the 5-finger flex sensors is:

| Sensor | Finger | ESP32-S3 Pin | ADC Channel |
| :--- | :--- | :--- | :--- |
| **Channel 0** | Thumb | **GPIO 1** | ADC1_CH0 |
| **Channel 1** | Index | **GPIO 2** | ADC1_CH1 |
| **Channel 2** | Middle | **GPIO 3** | ADC1_CH2 |
| **Channel 3** | Ring | **GPIO 4** | ADC1_CH3 |
| **Channel 4** | Pinky | **GPIO 5** | ADC1_CH4 |

- **Baud Rate:** 115200 (via USB-Serial/JTAG).
- **Sampling Frequency:** 10Hz (10 samples per second).

---

## 🔄 Complete Process Flow

### 1. Data Acquisition & Filtering
- **What:** The ESP32-S3 samples the 5 flex sensors via ADC1.
- **Processing:** An Exponential Moving Average (EMA) filter is applied to each channel with a smoothing factor ($\alpha$) of 0.2.
- **Why:** Flex sensors are susceptible to high-frequency electrical noise and mechanical vibration. EMA ensures a smooth signal without introducing significant latency.

### 2. Calibration & Normalization
- **What:** Tracks the minimum and maximum ADC values ever seen for each finger during the "Calibrate" mode.
- **Processing:** Raw 12-bit ADC values (0-4095) are mapped to a floating-point range (0.0 to 1.0).
- **Why:** Every hand is different, and flex sensors vary in resistance. Normalization ensures that "fully closed" is always 1.0 and "fully open" is always 0.0, making the gesture recognition logic universal across users.

### 3. Buffering (Time-Series Data)
- **What:** The system maintains a rolling buffer of the last 32 samples (approx. 3.2 seconds) for each of the 5 channels.
- **Processing:** This creates a feature vector of 160 elements ($5 \text{ channels} \times 32 \text{ samples}$).
- **Why:** Static finger positions aren't enough for robust gesture recognition; the temporal "signature" of how a gesture is held provides much higher accuracy.

### 4. On-Device Training (Storage)
- **What:** When a user triggers the `store` command (via the web UI), the current 160-element vector is appended to `/spiffs/dataset.bin` with a custom label (e.g., "Peace").
- **Why:** Storing data directly on the device allows it to function as a standalone peripheral. It doesn't need a cloud or PC to "remember" gestures after a reboot.

### 5. Inference (k-NN Engine)
- **What:** Every 1 second (during Infer Mode), the live buffer is compared against every record in the SPIFFS dataset.
- **Processing:** 
    - Uses **Euclidean Distance** to find the closest match.
    - Optimized with **ESP-DSP SIMD** instructions (`dsps_sub_f32` and `dsps_dotprod_f32`).
- **Why:** k-Nearest Neighbors is ideal for this scale. It requires no complex backpropagation or training phase. By using S3 SIMD instructions, we can compare dozens of gestures in under 1ms, ensuring zero-lag predictions.

### 6. Activity Detection (Disable Logic)
- **What:** Monitors the variance in the buffer. If the difference between min and max in a window is below a threshold (e.g., 100), the device marks itself as "Disabled."
- **Why:** Prevents "phantom" predictions when the glove is sitting still or not being worn.

---

## 📊 Communication Protocol

The ESP32 communicates with the web dashboard via a custom telemetry protocol over Serial:

- `>r[idx]:[val]`: Raw ADC value for a specific channel.
- `>[idx]:[val]`: Mapped (0-100) value for the UI progress bars.
- `>max[idx]:[val]` / `>min[idx]:[val]`: Current calibration bounds.
- `>mode:[MODE]`: Current operation state (TRAIN, INFER, CALIBRATE).
- `Best Match: [Label] (Dist: 0.00)`: Prediction results for the dashboard bubble.

---

## 🚀 Getting Started

1. **Hardware:** Connect 5 flex sensors to GPIOs 1-5 of an ESP32-S3.
2. **Build:** Use `idf.py build` in the `esp/` directory.
3. **Flash:** Use `idf.py flash monitor`.
4. **Dashboard:** Open the `frontend/` and run `bun dev` (or `npm run dev`) to access the Web Serial interface.
