# Smart Watch — ESP32 Health Tracker

A low-cost smart watch prototype built on the ESP32 that tracks **heart rate (BPM)**, **SpO₂**, and **steps**, with a three-screen UI on a 240×240 TFT display.

**Course:** CSC2104
**University:** Egyptian Chinese University (ECU) — Faculty of Computer & Information Systems
**Lecturers:** Dr. Haitham Farouk, Dr. Muhamed Abdulhadi
**TA:** Eng. Nada Abdelhamid

![Cover](images/cover.png)

## Idea
Premium smart watches cost roughly 10,000–20,000 EGP. This project targets essential health monitoring (heart rate and steps) with affordable components, aiming at a projected price of about 4,500 EGP.

## Features
- **Heart rate:** processes the raw IR signal from the MAX30102, detects beats, and averages them (moving average).
- **SpO₂:** ratio-of-ratios method with AC/DC component filtering.
- **Pedometer:** step detection from the MPU6050 with debounce and periodic auto-calibration.
- **Software RTC:** HH:MM:SS clock without an external RTC module.
- **UI:** three screens (Heart Rate, Activity, SpO₂) switched with a single button, using partial redraws to avoid flicker.
- **Startup self-check:** I²C scan to verify the sensors are connected.

## Hardware
| Part | Notes |
|---|---|
| ESP32 dev board | main controller |
| MAX30102 | heart rate / SpO₂ (I²C) |
| MPU6050 | motion / steps (I²C) |
| 240×240 TFT (ST7789) | SPI display |
| Li-Po battery 3.7 V | power |

## Pin mapping (from the firmware)
| Function | ESP32 pin |
|---|---|
| I²C SDA / SCL (MAX30102 + MPU6050) | GPIO 21 / 22 |
| TFT CS | GPIO 5 |
| TFT DC | GPIO 2 |
| TFT RST | GPIO 4 |
| TFT backlight | GPIO 12 |
| TFT MOSI / SCLK | GPIO 23 / 18 (default hardware SPI) |
| Screen-switch button | GPIO 0 (BOOT button) |

Sensors and display run on 3.3 V.

## Libraries
Install from the Arduino Library Manager:
- Adafruit GFX Library
- Adafruit ST7789 Library
- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Pulse and Proximity Sensor Library (provides `MAX30105.h`)

## Build & upload
1. Install the ESP32 board package in the Arduino IDE.
2. Open `firmware/smart_watch/smart_watch.ino`.
3. Select your ESP32 board and port, then upload.
4. Serial Monitor: 115200 baud.
5. Press the BOOT button to cycle between Heart Rate → Activity → SpO₂.

## Simulation / 3D visualization
`web/component-visualization.html` is a standalone page (three.js, loaded from a CDN) showing the components in 3D. Open it in a browser with internet access.

![Simulation](images/simulation.png)

## Docs
Project presentation: [`docs/Smart-Watch-presentation.pdf`](docs/Smart-Watch-presentation.pdf)

## Future work
- Waterproof casing (IP68)
- Mobile app over Bluetooth Low Energy for syncing and long-term health trends

## Team
Thomas Milad · Ali Fahmi Abolela · Mohamed Ali Elgazar · Mohamed Mahmoud · Ahmad Hamdy · Ziad Raed Ahmad · Kareem Ahmad · Fady Fouad · Saif Eldein Mohamed · Ahmad Mohamed Refaul · Kareem Mustafa · Ferial Mohamed · Nora Maher Mohamed · Noha Mohamed · Ebrahim Yossef Ebrahim · Mazen Ayman · Eslam Essam Sayed · Ahmed Hatem · Omar Ahmad Attia · Mohamed Sayed
