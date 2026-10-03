# ⌚ Smart Watch — ESP32 Health & Activity Monitor

A smartwatch-style prototype built on the **ESP32** that measures **heart rate (BPM)**, **SpO₂ (estimated)**, **steps**, and **movement**, and shows them on a color TFT display with a three-screen interface.

> ⚠️ **Disclaimer:** Educational prototype only. The readings are not clinically validated and must not be used for medical diagnosis or treatment.

---

## Features

- **Heart rate** — beat detection on the MAX30102 IR signal with a dynamic threshold, valid range 40–200 BPM, averaged over the last 8 beats
- **SpO₂ estimation** — ratio-of-ratios method with DC/AC filtering and a smoothing buffer
- **Step counter** — MPU6050 acceleration magnitude, dual thresholds, 250 ms debounce, automatic threshold calibration
- **Movement detection** — adjustable sensitivity (×0.5 to ×5.0)
- **Software clock** — HH:MM:SS driven by `millis()` (no RTC module)
- **3 screens** — Heart Rate, Activity, SpO₂; switch with the BOOT button; partial redraws to reduce flicker
- **Startup self-check** — I²C scan and automatic MAX30102 / MPU6050 detection (MPU6050 at `0x68` or `0x69`)
- **Serial Monitor commands** — help, reset, set time, tune sensitivity, debug

## Hardware

| Component | Purpose |
|---|---|
| ESP32 dev board | Main controller |
| MAX30102 | Heart rate & SpO₂ (I²C, `0x57`) |
| MPU6050 | Accelerometer / gyroscope (I²C, `0x68` / `0x69`) |
| ST7789 TFT, 172×320 | Display (SPI) |
| BOOT button (GPIO 0) | Screen switching |
| Jumper wires, USB cable | Connections, power & upload |

## Wiring

| Function | ESP32 pin |
|---|---|
| I²C SDA (MAX30102 + MPU6050) | GPIO 21 |
| I²C SCL (MAX30102 + MPU6050) | GPIO 22 |
| TFT CS | GPIO 5 |
| TFT DC | GPIO 2 |
| TFT RST | GPIO 4 |
| TFT backlight | GPIO 12 |
| TFT MOSI / SCLK | GPIO 23 / 18 (default hardware SPI) |
| Screen-switch button | GPIO 0 (BOOT) |

Both sensors share the same I²C bus (400 kHz).

## Libraries

Install from the Arduino Library Manager:

- Adafruit GFX Library
- Adafruit ST7789 Library
- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Pulse and Proximity Sensor Library (provides `MAX30105.h`)

## Build & Upload

1. Install the **ESP32 board package** in the Arduino IDE.
2. Open `smart_watch.ino`.
3. Select your board (e.g. *ESP32 Dev Module*) and the COM port.
4. Click **Upload**.
5. Open the Serial Monitor at **115200 baud**.

## Usage

Press the **BOOT** button to cycle: **Heart Rate → Activity → SpO₂ → Heart Rate**.
For heart rate and SpO₂, place your finger firmly and steadily on the MAX30102.

### Serial commands

| Command | Action |
|---|---|
| `h` | Show help |
| `r` | Reset system (heart rate, SpO₂, steps, clock, display) |
| `z` | Reset step counter to 0 |
| `+` | Add 10 steps (for testing) |
| `d` | Toggle debug output interval (500 ms / 2000 ms) |
| `time:HH,MM,SS` | Set the clock, 24-hour format, e.g. `time:14,30,00` |
| `s+` / `s-` | Increase / decrease MPU6050 sensitivity |

## How it works

**Heart rate.** The IR signal is buffered and a dynamic threshold is computed. A beat is registered when the signal rises above the threshold after being below it. BPM comes from the time between beats (accepted only if the interval is 300–2000 ms and the result is 40–200 BPM), then averaged over the last 8 valid beats. A finger is detected when the IR value exceeds `FINGER_THRESHOLD` (30000).

**SpO₂.** Red and IR signals are split into DC (low-pass filtered) and AC components, then:

```text
R    = (Red AC / Red DC) / (IR AC / IR DC)
SpO₂ = 110 − 25 × R
```

Results are smoothed over a 10-sample buffer.

**Steps.** Total acceleration is computed from X/Y/Z and smoothed over a history buffer. A step is counted on a high/low threshold crossing (defaults 11.5 / 9.0 m/s²) with a 250 ms debounce, and the thresholds are recalibrated automatically during movement.

### Update intervals

| Function | Interval |
|---|---:|
| Sensor read | 100 ms |
| Finger detection | 200 ms |
| BPM display | 500 ms |
| Steps display | 500 ms |
| Clock | 1000 ms |
| SpO₂ | 1000 ms |

## Limitations

- **SpO₂ is a simplified estimate**, not a validated algorithm. The output is also clamped to **95–100%** in the current code, so the yellow (90–94%) and red (<90%) display states are defined in the UI but cannot be reached yet.
- **Step counting is threshold-based** and can give false or missed steps depending on how the device is worn and moved.
- **The clock is software-only.** It starts at 07:07 on boot (12:00:00 after a reset command) and must be set again with `time:HH,MM,SS` after every restart.
- Heart rate accuracy depends heavily on finger pressure and stillness.

## Troubleshooting

| Problem | Check |
|---|---|
| MAX30102 not detected | Serial I²C scan should show `0x57`; check VCC, GND, SDA, SCL |
| MPU6050 not detected | Should appear at `0x68` or `0x69`; check wiring and AD0 pin |
| No heart rate value | Finger placement/pressure, sensor power, `FINGER_THRESHOLD` |
| Wrong step count | Try `s+` / `s-`; check sensor orientation |
| Blank display | TFT wiring (CS 5, DC 2, RST 4, BL 12), correct board selected |

## Future work

- Real RTC module (e.g. DS3231) for persistent time
- Proper SpO₂ calibration and an improved signal-processing pipeline
- Better step-detection algorithm and persistent step storage
- Battery monitoring and power optimization
- Bluetooth Low Energy / Wi-Fi sync with a mobile app
- Data logging and historical health charts
- Waterproof casing

## Tech stack

C++ · Arduino · ESP32 · I²C · SPI · MAX30102 · MPU6050 · ST7789 · Adafruit GFX · Embedded signal processing

## Project structure

```text
Smart-Watch/
├── smart_watch.ino
└── README.md
```
