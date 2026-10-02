#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Wire.h>
#include "MAX30105.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// ========== Display Pins ==========
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   12

// ========== Button Pin ==========
#define BOOT_BUTTON 0  // ESP32 BOOT button on GPIO0

// ========== I2C Pins ==========
#define I2C_SDA 21
#define I2C_SCL 22

// ========== Thresholds ==========
#define FINGER_THRESHOLD 30000  // Lowered for better detection
#define MIN_BPM 40
#define MAX_BPM 200
#define MOVEMENT_THRESHOLD 0.3  // More sensitive
#define STEP_THRESHOLD 1.0      // More sensitive
#define STEP_DEBOUNCE_MS 250    // Faster step detection

// ========== UPDATE RATES ==========
#define CLOCK_UPDATE_INTERVAL 1000   // Update clock every second
#define BPM_UPDATE_INTERVAL 500      // Update BPM every 500ms
#define FINGER_UPDATE_INTERVAL 200   // Update finger status every 200ms
#define STEPS_UPDATE_INTERVAL 500    // Update steps every 500ms
#define SENSOR_UPDATE_INTERVAL 100   // More frequent sensor updates
#define SPO2_UPDATE_INTERVAL 1000    // Update SpO2 every second

// ========== SpO2 Calibration Constants ==========
#define CALIBRATION_SAMPLES 50       // Samples for SpO2 calibration
#define SPO2_LOW_LIMIT 95
#define SPO2_HIGH_LIMIT 100
#define DC_FILTER_ALPHA 0.95         // For DC component filtering
#define AC_FILTER_ALPHA 0.5          // For AC component filtering

// ========== Objects ==========
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
MAX30105 particleSensor;
Adafruit_MPU6050 mpu;

// ========== Heart Rate Variables ==========
const byte RATE_SIZE = 8;  // Increased for better averaging
byte rates[RATE_SIZE];
byte rateSpot = 0;
long irBuffer[100];  // Increased buffer for better signal processing
byte bufferIndex = 0;
long irAverage = 0;
long irMax = 0;
long irMin = 999999;
unsigned long lastBeatTime = 0;
long irValue = 0;
long redValue = 0;
int beatsPerMinute = 0;
int beatAvg = 0;

// ========== SpO2 Variables ==========
float spo2 = 0.0;
float redDc = 0.0;
float irDc = 0.0;
float redAc = 0.0;
float irAc = 0.0;
float rValue = 0.0;
float spo2Buffer[10];
byte spo2BufferIndex = 0;
bool spo2Valid = false;
unsigned long lastSpo2Update = 0;
float calibrationR = 0.0;
int calibrationSamples = 0;
bool isCalibrating = false;

// ========== MPU6050 Variables ==========
float accelX = 0, accelY = 0, accelZ = 0;
float gyroX = 0, gyroY = 0, gyroZ = 0;
float totalAccel = 0;
float movementMagnitude = 0;
bool isMoving = false;
unsigned long stepCount = 0;
unsigned long lastStepTime = 0;
float accelHistory[20];  // Increased history for better smoothing
byte accelHistoryIndex = 0;
float lastTotalAccel = 9.8;
byte mpuAddress = 0;
bool useDirectMPU = false;
float accelSensitivity = 2.0;  // Increased sensitivity factor

// ========== Step Detection Variables ==========
float stepThresholdHigh = 11.5;  // Adjusted for better sensitivity
float stepThresholdLow = 9.0;    // Adjusted for better sensitivity
bool stepStateHigh = false;
unsigned long lastStepDetected = 0;
float stepBuffer[10];  // Larger buffer for better smoothing
byte stepBufferIndex = 0;

// ========== System Variables ==========
bool sensorConnected = false;
bool mpuConnected = false;
bool fingerDetected = false;
unsigned long lastClockUpdate = 0;
unsigned long lastBpmUpdate = 0;
unsigned long lastFingerUpdate = 0;
unsigned long lastStepsUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastSpo2DisplayUpdate = 0;
unsigned long lastDebugPrint = 0;
unsigned long lastButtonCheck = 0;
int screenMode = 0;  // 0 = Heart Rate, 1 = Activity, 2 = SpO2
unsigned long debugPrintInterval = 2000;  // More frequent debug
int lastScreenMode = -1;  // Track last screen mode for redraw

// ========== Time Variables ==========
int currentHour = 7;
int currentMinute = 07;
int currentSecond = 0;
unsigned long lastMillis = 0;

// ========== Button Variables ==========
bool buttonPressed = false;
bool lastButtonState = true;
unsigned long lastDebounceTime = 0;
#define DEBOUNCE_DELAY 50

// ========== Display State Variables ==========
bool screenInitialized = false;
int lastDisplayedBeatAvg = -1;
bool lastDisplayedFingerDetected = false;
unsigned long lastDisplayedStepCount = 0;
bool lastDisplayedIsMoving = false;
String lastDisplayedTime = "";
float lastDisplayedSpo2 = -1.0;
bool lastDisplayedSpo2Valid = false;

// ========== Professional Color Scheme ==========
#define BG_DARK 0x0000      // Black
#define BG_MID 0x18C3       // Dark Blue-Gray
#define BG_LIGHT 0x8410     // Medium Gray
#define TXT_WHITE 0xFFFF    // White
#define TXT_GRAY 0x738E     // Light Gray
#define HEART_RED 0xF800    // Red for heart rate
#define ACCENT_BLUE 0x041F  // Blue accent
#define ACCENT_GREEN 0x07E0 // Green for OK
#define WARNING_YELLOW 0xFFE0 // Yellow for warnings
#define STEP_BLUE 0x051F    // Blue for step counter
#define CLOCK_COLOR 0x07FF  // Cyan for clock
#define SPO2_BLUE 0x039F    // Blue for SpO2
#define OXYGEN_CYAN 0x07FF  // Cyan for oxygen level

// ========== UI Elements ==========
#define CORNER_RADIUS 8

// ========== Function Declarations ==========
void scanI2C();
bool initSensor();
bool initMPU();
void readSensor();
void readMPU();
void readMPUDirect();
void detectBeat();
void detectMovement();
void detectStep();
void updateClock();
void updateDisplay();
void initializeScreen();
void drawHeartRateScreen();
void drawActivityScreen();
void drawSpO2Screen();  // New function for SpO2 screen
void drawStatusBar();
void updateStatusBar();
void updateBpmDisplay();
void updateClockDisplay();
void updateFingerStatus();
void updateStepsDisplay();
void updateSpO2Display();  // New function for SpO2 display
void drawCard(int x, int y, int w, int h, uint16_t color);
void handleSerialCommands();
void handleButtonPress();
void resetAll();
bool testMPUAddress(uint8_t address);
bool directMPUInit();
bool wakeUpMPU(uint8_t address);
float getSmoothedAccel();
void calibrateStepThresholds();
String formatTime(int hour, int minute, int second);
void calculateSpO2();  // New function for SpO2 calculation
void updateSpo2Calibration();  // New function for calibration
void drawSpO2Value();  // New function to draw SpO2 value
void increaseMPUSensitivity();  // New function to increase MPU sensitivity

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n========================================");
  Serial.println("Health Monitor Pro v2.0");
  Serial.println("MAX30102 + MPU6050 + SpO2");
  Serial.println("========================================\n");
  
  // Initialize BOOT button
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  tft.init(172, 320);
  tft.setRotation(0);
  tft.fillScreen(BG_DARK);
  
  // Draw professional splash screen
  drawCard(10, 60, 152, 100, BG_MID);
  
  tft.setTextColor(HEART_RED);
  tft.setTextSize(2);
  tft.setCursor(40, 90);
  tft.print("Health");
  tft.setCursor(60, 120);
  tft.print("Pro");
  
  tft.setTextColor(SPO2_BLUE);
  tft.setTextSize(1);
  tft.setCursor(30, 150);
  tft.print("HR + SpO2 + Steps");
  
  delay(2000);
  
  lastMillis = millis();
  
  Serial.println("Initializing I2C...");
  Wire.begin(I2C_SDA, I2C_SCL, 400000);  // Increased I2C speed
  delay(200);
  
  tft.fillScreen(BG_DARK);
  drawCard(10, 50, 152, 60, BG_MID);
  tft.setTextColor(TXT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(50, 75);
  tft.print("Initializing...");
  
  scanI2C();
  delay(500);
  
  sensorConnected = initSensor();
  
  tft.fillScreen(BG_DARK);
  tft.setCursor(30, 80);
  if (sensorConnected) {
    tft.setTextColor(ACCENT_GREEN);
    tft.print("MAX30102: Connected");
    Serial.println("MAX30102: Connected");
  } else {
    tft.setTextColor(WARNING_YELLOW);
    tft.print("MAX30102: Not Found");
    Serial.println("MAX30102: Not Found");
  }
  
  delay(500);
  
  if (mpuAddress == 0) {
    Serial.println("\nTrying to find MPU6050...");
    if (testMPUAddress(0x68)) {
      mpuAddress = 0x68;
      Serial.println("Found MPU at 0x68");
    } else if (testMPUAddress(0x69)) {
      mpuAddress = 0x69;
      Serial.println("Found MPU at 0x69");
    }
  }
  
  if (mpuAddress != 0) {
    Serial.print("\nInitializing MPU at 0x");
    Serial.println(mpuAddress, HEX);
    
    mpuConnected = initMPU();
    
    if (!mpuConnected) {
      Serial.println("Library init failed, trying direct mode...");
      mpuConnected = directMPUInit();
      if (mpuConnected) {
        useDirectMPU = true;
        Serial.println("Using direct MPU communication");
      }
    }
    
    // Increase MPU sensitivity
    if (mpuConnected) {
      increaseMPUSensitivity();
    }
  }
  
  tft.setCursor(30, 100);
  if (mpuConnected) {
    tft.setTextColor(ACCENT_GREEN);
    tft.print("MPU6050: Connected");
    Serial.println("MPU6050: Connected");
    
    for (int i = 0; i < 10; i++) {
      stepBuffer[i] = 9.8;
    }
  } else {
    tft.setTextColor(WARNING_YELLOW);
    tft.print("MPU6050: Not Found");
    Serial.println("MPU6050: Not Found");
  }
  
  delay(1000);
  
  // Draw final ready screen
  tft.fillScreen(BG_DARK);
  drawCard(30, 100, 112, 60, BG_MID);
  
  tft.setTextColor((sensorConnected && mpuConnected) ? ACCENT_GREEN : WARNING_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(50, 120);
  tft.print("READY");
  
  tft.setTextColor(TXT_GRAY);
  tft.setTextSize(1);
  tft.setCursor(35, 150);
  tft.print("Press BOOT Button");
  tft.setCursor(40, 165);
  tft.print("to Switch Screens");
  
  delay(1500);
  
  // Initialize screen
  initializeScreen();
  
  Serial.println("\nSystem Ready!");
  Serial.println("\n=== CONTROLS ===");
  Serial.println("BOOT Button - Switch screen (HR/Activity/SpO2)");
  Serial.println("Serial Commands:");
  Serial.println("  h - Show help");
  Serial.println("  r - Reset system");
  Serial.println("  z - Reset step counter to 0");
  Serial.println("  + - Add 10 steps");
  Serial.println("  d - Toggle debug mode");
  Serial.println("  time:HH,MM,SS - Set time (e.g., time:14,30,0)");
  Serial.println("  s+ - Increase MPU sensitivity");
  Serial.println("  s- - Decrease MPU sensitivity");
  Serial.println("========================\n");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Update time (keeps running even when display not updating)
  updateClock();
  
  // Update sensors
  if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    lastSensorUpdate = currentMillis;
    
    readSensor();
    
    if (mpuConnected) {
      if (useDirectMPU) {
        readMPUDirect();
      } else {
        readMPU();
      }
      
      // Apply sensitivity multiplier
      totalAccel = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ) * accelSensitivity;
      
      movementMagnitude = abs(totalAccel - lastTotalAccel);
      lastTotalAccel = totalAccel;
      
      accelHistory[accelHistoryIndex] = totalAccel;
      accelHistoryIndex = (accelHistoryIndex + 1) % 20;
      
      detectMovement();
      detectStep();
    }
    
    if (fingerDetected) {
      detectBeat();
      
      // Calculate SpO2 less frequently
      if (currentMillis - lastSpo2Update >= SPO2_UPDATE_INTERVAL) {
        lastSpo2Update = currentMillis;
        calculateSpO2();
      }
    } else {
      spo2Valid = false;
      spo2 = 0;
    }
  }
  
  // Check BOOT button
  handleButtonPress();
  
  // Check if screen mode has changed - force redraw if it has
  if (screenMode != lastScreenMode) {
    initializeScreen();
    lastScreenMode = screenMode;
  }
  
  // Update clock display every second
  if (currentMillis - lastClockUpdate >= CLOCK_UPDATE_INTERVAL) {
    lastClockUpdate = currentMillis;
    updateClockDisplay();
  }
  
  // Update BPM display every 500ms (only if on heart rate screen)
  if (screenMode == 0 && currentMillis - lastBpmUpdate >= BPM_UPDATE_INTERVAL) {
    lastBpmUpdate = currentMillis;
    updateBpmDisplay();
  }
  
  // Update finger status every 200ms (only if on heart rate screen)
  if (screenMode == 0 && currentMillis - lastFingerUpdate >= FINGER_UPDATE_INTERVAL) {
    lastFingerUpdate = currentMillis;
    updateFingerStatus();
  }
  
  // Update steps display every 500ms
  if (currentMillis - lastStepsUpdate >= STEPS_UPDATE_INTERVAL) {
    lastStepsUpdate = currentMillis;
    updateStepsDisplay();
  }
  
  // Update SpO2 display every second (only if on SpO2 screen)
  if (screenMode == 2 && currentMillis - lastSpo2DisplayUpdate >= SPO2_UPDATE_INTERVAL) {
    lastSpo2DisplayUpdate = currentMillis;
    updateSpO2Display();
  }
  
  if (currentMillis - lastDebugPrint >= debugPrintInterval) {
    lastDebugPrint = currentMillis;
    Serial.print("HR: ");
    Serial.print(beatAvg > 0 ? String(beatAvg) : "--");
    Serial.print(" | SpO2: ");
    Serial.print(spo2Valid ? String(spo2, 1) : "--");
    Serial.print("% | Steps: ");
    Serial.print(stepCount);
    Serial.print(" | Accel: ");
    Serial.print(totalAccel, 2);
    Serial.print(" | Finger: ");
    Serial.print(fingerDetected ? "YES" : "NO");
    Serial.print(" | Sensitivity: x");
    Serial.print(accelSensitivity, 1);
    Serial.print(" | Time: ");
    Serial.print(formatTime(currentHour, currentMinute, currentSecond));
    Serial.println();
  }
  
  handleSerialCommands();
  
  // Main loop delay (reduced for more responsive UI)
  delay(30);
}

// ========== TIME FUNCTIONS ==========

void updateClock() {
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - lastMillis;
  
  if (elapsedMillis >= 1000) {
    currentSecond++;
    lastMillis = currentMillis - (elapsedMillis - 1000);
    
    if (currentSecond >= 60) {
      currentSecond = 0;
      currentMinute++;
      
      if (currentMinute >= 60) {
        currentMinute = 0;
        currentHour++;
        
        if (currentHour >= 24) {
          currentHour = 0;
        }
      }
    }
  }
}

String formatTime(int hour, int minute, int second) {
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", hour, minute, second);
  return String(timeStr);
}

// ========== BUTTON HANDLING ==========

void handleButtonPress() {
  int buttonState = digitalRead(BOOT_BUTTON);
  
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (buttonState == LOW && !buttonPressed) {
      buttonPressed = true;
      
      // Switch screen through 3 modes
      screenMode = (screenMode + 1) % 3;
      
      const char* screenNames[] = {"Heart Rate", "Activity", "SpO2"};
      Serial.print("Button pressed! Switched to ");
      Serial.println(screenNames[screenMode]);
      
      // Redraw entire screen for new mode
      initializeScreen();
    }
    
    if (buttonState == HIGH) {
      buttonPressed = false;
    }
  }
  
  lastButtonState = buttonState;
}

// ========== DISPLAY FUNCTIONS ==========

void initializeScreen() {
  screenInitialized = false;
  tft.fillScreen(BG_DARK);
  drawStatusBar();
  
  if (screenMode == 0) {
    drawHeartRateScreen();
  } else if (screenMode == 1) {
    drawActivityScreen();
  } else {
    drawSpO2Screen();
  }
  
  screenInitialized = true;
  
  // Reset display state variables
  lastDisplayedBeatAvg = -1;
  lastDisplayedFingerDetected = false;
  lastDisplayedStepCount = 0;
  lastDisplayedIsMoving = false;
  lastDisplayedTime = "";
  lastDisplayedSpo2 = -1.0;
  lastDisplayedSpo2Valid = false;
}

void updateClockDisplay() {
  // Only update the clock part of status bar
  String currentTime = formatTime(currentHour, currentMinute, currentSecond);
  
  if (currentTime != lastDisplayedTime) {
    // Clear previous clock area
    tft.fillRect(50, 5, 70, 10, BG_MID);
    
    // Draw new clock
    tft.setTextColor(CLOCK_COLOR);
    tft.setTextSize(1);
    tft.setCursor(50, 5);
    tft.print(currentTime);
    
    lastDisplayedTime = currentTime;
  }
}

void updateBpmDisplay() {
  if (screenMode != 0) return; // Only update on heart rate screen
  
  if (beatAvg != lastDisplayedBeatAvg) {
    // Clear BPM area
    tft.fillRect(50, 145, 70, 30, BG_MID);
    
    // Draw BPM value
    tft.setTextSize(3);
    tft.setCursor(50, 145);
    
    if (beatAvg > 0 && fingerDetected) {
      tft.setTextColor(HEART_RED);
      tft.print(beatAvg);
    } else {
      tft.setTextColor(TXT_GRAY);
      tft.print("--");
    }
    
    lastDisplayedBeatAvg = beatAvg;
  }
}

void updateFingerStatus() {
  if (screenMode != 0) return; // Only update on heart rate screen
  
  if (fingerDetected != lastDisplayedFingerDetected) {
    // Clear finger status area
    tft.fillRect(70, 220, 80, 10, BG_MID);
    
    // Draw new finger status
    tft.setTextSize(1);
    tft.setCursor(70, 220);
    tft.setTextColor(fingerDetected ? ACCENT_GREEN : WARNING_YELLOW);
    tft.print(fingerDetected ? "DETECTED" : "PLACE FINGER");
    
    // Update heart icon color
    uint16_t heartColor = fingerDetected && beatAvg > 0 ? HEART_RED : TXT_GRAY;
    tft.fillCircle(67, 110, 5, heartColor);
    tft.fillCircle(73, 110, 5, heartColor);
    tft.fillTriangle(62, 115, 78, 115, 70, 125, heartColor);
    
    lastDisplayedFingerDetected = fingerDetected;
  }
}

void updateStepsDisplay() {
  if (stepCount != lastDisplayedStepCount) {
    // Update steps based on current screen
    if (screenMode == 0) {
      // Heart Rate screen - update steps in info card
      tft.setTextSize(1);
      tft.setTextColor(TXT_WHITE);
      tft.setCursor(60, 260);
      tft.fillRect(60, 260, 40, 10, BG_MID);
      tft.print(stepCount);
    } else if (screenMode == 1) {
      // Activity screen - update big steps counter
      tft.setTextColor(STEP_BLUE);
      tft.setTextSize(4);
      tft.setCursor(40, 135);
      tft.fillRect(40, 135, 90, 30, BG_MID);
      tft.print(stepCount);
    }
    
    lastDisplayedStepCount = stepCount;
  }
}

void updateSpO2Display() {
  if (screenMode != 2) return; // Only update on SpO2 screen
  
  // Check if value has changed significantly (more than 0.1%) or validity changed
  if (fabs(spo2 - lastDisplayedSpo2) > 0.1 || spo2Valid != lastDisplayedSpo2Valid) {
    drawSpO2Value();
    lastDisplayedSpo2 = spo2;
    lastDisplayedSpo2Valid = spo2Valid;
  }
}

void drawStatusBar() {
  // Top status bar
  tft.fillRect(0, 0, 172, 25, BG_MID);
  
  // Screen indicator
  tft.setTextColor(TXT_GRAY);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  if (screenMode == 0) {
    tft.setTextColor(HEART_RED);
    tft.print("HEART");
  } else if (screenMode == 1) {
    tft.setTextColor(STEP_BLUE);
    tft.print("ACTIVITY");
  } else {
    tft.setTextColor(SPO2_BLUE);
    tft.print("SpO2");
  }
  
  // Cairo time clock
  tft.setTextColor(CLOCK_COLOR);
  tft.setCursor(50, 5);
  tft.print(formatTime(currentHour, currentMinute, currentSecond));
  
  // Sensor status
  tft.setCursor(50, 15);
  tft.setTextColor(sensorConnected ? ACCENT_GREEN : WARNING_YELLOW);
  tft.print(sensorConnected ? "SENSOR OK" : "NO SENSOR");
  
  // Sensitivity indicator
  if (mpuConnected) {
    tft.setCursor(140, 5);
    tft.setTextColor(TXT_GRAY);
    tft.print("x");
    tft.print(accelSensitivity, 1);
  }
  
  lastDisplayedTime = formatTime(currentHour, currentMinute, currentSecond);
}

void drawHeartRateScreen() {
  // Clear only main area (not status bar)
  tft.fillRect(0, 25, 172, 295, BG_DARK);
  
  // Header
  drawCard(10, 30, 152, 40, BG_MID);
  tft.setTextColor(TXT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(40, 45);
  tft.print("Heart Rate");
  
  // Main heart rate display
  drawCard(10, 80, 152, 120, BG_MID);
  
  // Draw heart icon
  uint16_t heartColor = fingerDetected && beatAvg > 0 ? HEART_RED : TXT_GRAY;
  tft.fillCircle(67, 110, 5, heartColor);
  tft.fillCircle(73, 110, 5, heartColor);
  tft.fillTriangle(62, 115, 78, 115, 70, 125, heartColor);
  
  // BPM label
  tft.setTextSize(1);
  tft.setTextColor(TXT_GRAY);
  tft.setCursor(75, 175);
  tft.print("BPM");
  
  // Finger detection status
  drawCard(10, 210, 152, 30, BG_MID);
  tft.setTextSize(1);
  tft.setCursor(20, 220);
  tft.print("Finger: ");
  
  // Additional info card
  drawCard(10, 250, 152, 60, BG_MID);
  
  tft.setTextColor(TXT_GRAY);
  tft.setCursor(20, 260);
  tft.print("Steps: ");
  
  tft.setCursor(20, 275);
  tft.print("SpO2: ");
  
  // Signal status
  tft.setCursor(100, 260);
  tft.setTextColor(TXT_GRAY);
  tft.print("IR: ");
  tft.setTextColor(irValue > 10000 ? ACCENT_GREEN : WARNING_YELLOW);
  tft.print(irValue);
  
  // Instructions
  drawCard(10, 320, 152, 20, BG_MID);
  tft.setTextColor(TXT_GRAY);
  tft.setCursor(20, 326);
  tft.print("Press BOOT to switch");
  
  // Initialize state variables
  lastDisplayedFingerDetected = fingerDetected;
  lastDisplayedStepCount = stepCount;
  
  // Draw initial values
  updateBpmDisplay();
  updateFingerStatus();
  updateStepsDisplay();
  
  // Draw initial SpO2 value
  tft.setTextSize(1);
  tft.setCursor(60, 275);
  tft.fillRect(60, 275, 40, 10, BG_MID);
  if (spo2Valid && fingerDetected) {
    tft.setTextColor(SPO2_BLUE);
    tft.print(spo2, 1);
    tft.print("%");
  } else {
    tft.setTextColor(TXT_GRAY);
    tft.print("--");
  }
}

void drawActivityScreen() {
  // Clear only main area (not status bar)
  tft.fillRect(0, 25, 172, 295, BG_DARK);
  
  // Header
  drawCard(10, 30, 152, 40, BG_MID);
  tft.setTextColor(TXT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(45, 45);
  tft.print("Activity");
  
  // Steps counter
  drawCard(10, 80, 152, 100, BG_MID);
  
  // Step icon
  uint16_t stepColor = STEP_BLUE;
  tft.fillCircle(70, 110, 6, stepColor);
  tft.fillCircle(78, 105, 4, stepColor);
  for (int i = 0; i < 3; i++) {
    tft.fillCircle(84, 102 + i*4, 2, stepColor);
  }
  
  // Steps label
  tft.setTextSize(1);
  tft.setTextColor(TXT_GRAY);
  tft.setCursor(70, 165);
  tft.print("STEPS");
  
  // Activity status
  drawCard(10, 190, 152, 30, BG_MID);
  
  // Movement icon
  uint16_t moveColor = isMoving ? ACCENT_GREEN : TXT_GRAY;
  tft.fillCircle(24, 200, 3, moveColor);
  tft.drawLine(24, 203, 24, 210, moveColor);
  tft.drawLine(24, 206, 20, 203, moveColor);
  tft.drawLine(24, 206, 28, 203, moveColor);
  tft.drawLine(24, 210, 20, 215, moveColor);
  tft.drawLine(24, 210, 28, 215, moveColor);
  
  tft.setTextSize(1);
  tft.setCursor(45, 205);
  tft.print("Status: ");
  tft.setTextColor(isMoving ? ACCENT_GREEN : TXT_GRAY);
  tft.print(isMoving ? "MOVING" : "IDLE");
  lastDisplayedIsMoving = isMoving;
  
  // Acceleration data
  drawCard(10, 230, 152, 80, BG_MID);
  
  tft.setTextColor(TXT_GRAY);
  tft.setCursor(20, 240);
  tft.print("Acceleration: ");
  tft.setTextColor(TXT_WHITE);
  tft.print(totalAccel, 1);
  tft.print(" m/s²");
  
  tft.setCursor(20, 260);
  tft.setTextColor(TXT_GRAY);
  tft.print("Sensitivity: x");
  tft.setTextColor(SPO2_BLUE);
  tft.print(accelSensitivity, 1);
  
  tft.setCursor(20, 280);
  tft.setTextColor(TXT_GRAY);
  tft.print("Steps Today: ");
  tft.setTextColor(TXT_WHITE);
  tft.print(stepCount);
  
  // Initialize state variable
  lastDisplayedStepCount = stepCount;
  
  // Draw initial steps count
  updateStepsDisplay();
}

void drawSpO2Screen() {
  // Clear only main area (not status bar)
  tft.fillRect(0, 25, 172, 295, BG_DARK);
  
  // Header
  drawCard(10, 30, 152, 40, BG_MID);
  tft.setTextColor(TXT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(55, 45);
  tft.print("SpO2");
  
  // Main SpO2 display - adjusted height to prevent overlap
  drawCard(10, 80, 152, 90, BG_MID);
  
  // Oxygen molecule icon
  tft.fillCircle(70, 110, 10, SPO2_BLUE);
  tft.fillCircle(90, 110, 8, OXYGEN_CYAN);
  
  // Draw bond between atoms
  for (int i = 70; i <= 90; i++) {
    if (i % 3 == 0) {
      tft.drawPixel(i, 110, TXT_WHITE);
    }
  }
  
  // SpO2 label
  tft.setTextSize(1);
  tft.setTextColor(TXT_GRAY);
  tft.setCursor(65, 155);
  tft.print("BLOOD OXYGEN");
  
  // Draw initial SpO2 value (will show -- initially)
  // This area will be cleared by drawSpO2Value()
  tft.setTextSize(3);
  tft.setCursor(45, 120);
  tft.setTextColor(TXT_GRAY);
  tft.print("--");
  
  // Information Card - positioned to prevent overlap
  drawCard(10, 180, 152, 100, BG_MID);
  
  // Info lines with clearer labels and values
  tft.setTextColor(TXT_GRAY);
  tft.setTextSize(1);
  
  // Line 1: Heart Rate
  tft.setCursor(20, 195);
  tft.print("Heart Rate:");
  tft.setCursor(100, 195);
  tft.setTextColor(HEART_RED);
  tft.print(beatAvg > 0 ? String(beatAvg) : "--");
  tft.print(" BPM");
  
  // Line 2: IR Signal
  tft.setCursor(20, 215);
  tft.setTextColor(TXT_GRAY);
  tft.print("IR Signal:");
  tft.setCursor(100, 215);
  if(irValue > 10000) {
    tft.setTextColor(ACCENT_GREEN);
  } else {
    tft.setTextColor(WARNING_YELLOW);
  }
  tft.print(irValue/1000); // Display in thousands
  tft.print("k");
  
  // Line 3: Red Signal
  tft.setCursor(20, 235);
  tft.setTextColor(TXT_GRAY);
  tft.print("Red Signal:");
  tft.setCursor(100, 235);
  if(redValue > 5000) {
    tft.setTextColor(ACCENT_GREEN);
  } else {
    tft.setTextColor(WARNING_YELLOW);
  }
  tft.print(redValue/1000);
  tft.print("k");
  
  // Line 4: R-Value (for debugging SpO2 calculation)
  tft.setCursor(20, 255);
  tft.setTextColor(TXT_GRAY);
  tft.print("R-Value:");
  tft.setCursor(100, 255);
  tft.setTextColor(TXT_WHITE);
  tft.print(rValue, 3);
  
  // Instructions Bar at the bottom
  drawCard(10, 290, 152, 20, BG_MID);
  tft.setTextColor(TXT_GRAY);
  tft.setTextSize(1);
  tft.setCursor(25, 296);
  tft.print("Place finger firmly");
  
  // Reset display state to force update
  lastDisplayedSpo2 = -1.0;
  lastDisplayedSpo2Valid = false;
}

void drawSpO2Value() {
  if (screenMode != 2) return; // Only draw if on SpO2 screen
  
  // Define the clear area for the SpO2 value
  int valueX = 40;
  int valueY = 120;
  int valueWidth = 90;
  int valueHeight = 30;
  
  // Clear the previous value area
  tft.fillRect(valueX, valueY, valueWidth, valueHeight, BG_MID);
  
  tft.setTextSize(3);
  tft.setCursor(valueX, valueY);
  
  if (spo2Valid && fingerDetected && spo2 > 1.0) {
    // Color coding based on health guidelines
    if (spo2 >= 95.0) {
      tft.setTextColor(ACCENT_GREEN); // Healthy: 95-100%
    } else if (spo2 >= 90.0) {
      tft.setTextColor(WARNING_YELLOW); // Caution: 90-94%
    } else {
      tft.setTextColor(HEART_RED); // Low: below 90%
    }
    
    // Display with one decimal place
    tft.print(spo2, 1);
    
    // Draw percentage symbol at size 2
    tft.setTextSize(2);
    // Position the % symbol after the number
    // 4 characters * 18 pixels (6*3) = 72 pixels
    tft.setCursor(valueX + 72, valueY + 5);
    tft.print("%");
  } else {
    // No valid reading
    tft.setTextColor(TXT_GRAY);
    tft.setTextSize(3);
    tft.setCursor(valueX + 20, valueY); // Center "--" in the area
    tft.print("--");
  }
}

// ========== HELPER FUNCTIONS ==========

void drawCard(int x, int y, int w, int h, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, CORNER_RADIUS, color);
}

// ========== SENSOR FUNCTIONS ==========

void scanI2C() {
  Serial.println("\nScanning I2C bus...");
  int count = 0;
  
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("  Device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      
      if (addr == 0x57) Serial.print(" (MAX30102)");
      if (addr == 0x68) {
        Serial.print(" (MPU6050)");
        mpuAddress = 0x68;
      }
      if (addr == 0x69) {
        Serial.print(" (MPU6050)");
        mpuAddress = 0x69;
      }
      
      Serial.println();
      count++;
    }
  }
  
  if (count == 0) {
    Serial.println("  *** NO I2C DEVICES FOUND! ***");
  }
}

bool initSensor() {
  Serial.println("\nInitializing MAX30102...");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("  FAILED!");
    return false;
  }
  
  // Configure sensor for both heart rate and SpO2
  byte ledBrightness = 0x1F;  // Options: 0=Off to 255=50mA
  byte sampleAverage = 4;     // Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2;           // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 100;       // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411;       // Options: 69, 118, 215, 411
  int adcRange = 4096;        // Options: 2048, 4096, 8192, 16384
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  // Set LED currents for SpO2 (higher for better signal)
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
  
  // Disable green LED if not used
  particleSensor.setPulseAmplitudeGreen(0);
  
  Serial.println("  OK!");
  return true;
}

bool testMPUAddress(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(0x75);
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    return false;
  }
  
  Wire.requestFrom(address, (uint8_t)1);
  if (!Wire.available()) {
    return false;
  }
  
  byte whoami = Wire.read();
  return (whoami == 0x68 || whoami == 0x71);
}

bool initMPU() {
  if (mpuAddress == 0) return false;
  
  Serial.print("\nInitializing MPU6050 at 0x");
  Serial.print(mpuAddress, HEX);
  Serial.println(" with library...");
  
  delay(100);
  
  if (!mpu.begin(mpuAddress, &Wire)) {
    Serial.println("  Library init failed");
    return false;
  }
  
  Serial.println("  Library init OK!");
  
  // Configure for higher sensitivity
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);  // More sensitive than 8_G
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);       // More sensitive gyro
  mpu.setFilterBandwidth(MPU6050_BAND_10_HZ);    // Lower bandwidth for less noise
  
  delay(100);
  
  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp)) {
    Serial.println("  Test read successful!");
    return true;
  }
  
  Serial.println("  Test read failed");
  return false;
}

void increaseMPUSensitivity() {
  if (useDirectMPU) {
    // Configure for ±2g range (most sensitive)
    Wire.beginTransmission(mpuAddress);
    Wire.write(0x1C); // Accelerometer config register
    Wire.write(0x00); // ±2g range (most sensitive)
    Wire.endTransmission();
    
    // Configure gyro for ±250°/s (most sensitive)
    Wire.beginTransmission(mpuAddress);
    Wire.write(0x1B); // Gyro config register
    Wire.write(0x00); // ±250°/s range
    Wire.endTransmission();
    
    // Configure digital low-pass filter
    Wire.beginTransmission(mpuAddress);
    Wire.write(0x1A); // DLPF config register
    Wire.write(0x05); // ~10Hz bandwidth (reduces noise)
    Wire.endTransmission();
    
    Serial.println("MPU sensitivity increased (Direct mode)");
  }
}

bool directMPUInit() {
  if (mpuAddress == 0) return false;
  
  Serial.print("\nInitializing MPU6050 at 0x");
  Serial.print(mpuAddress, HEX);
  Serial.println(" directly...");
  
  if (!wakeUpMPU(mpuAddress)) {
    Serial.println("  Failed to wake up MPU");
    return false;
  }
  
  // Configure for highest sensitivity
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x1C); // Accelerometer config
  Wire.write(0x00); // ±2g (most sensitive)
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.println("  Failed to configure accelerometer");
    return false;
  }
  
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x1B); // Gyro config
  Wire.write(0x00); // ±250°/s (most sensitive)
  error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.println("  Failed to configure gyro");
    return false;
  }
  
  // Configure digital low-pass filter for noise reduction
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x1A); // DLPF config
  Wire.write(0x05); // Bandwidth ~10Hz
  error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.println("  Failed to configure filter");
    return false;
  }
  
  delay(100);
  
  int16_t ax, ay, az;
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B);
  error = Wire.endTransmission(false);
  
  if (error == 0) {
    Wire.requestFrom(mpuAddress, (uint8_t)6);
    if (Wire.available() >= 6) {
      ax = Wire.read() << 8 | Wire.read();
      ay = Wire.read() << 8 | Wire.read();
      az = Wire.read() << 8 | Wire.read();
      
      Serial.println("  Direct read successful!");
      return true;
    }
  }
  
  Serial.println("  Direct read failed");
  return false;
}

bool wakeUpMPU(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(0x6B); // Power management register
  Wire.write(0x00); // Wake up
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    return false;
  }
  
  delay(100);
  return true;
}

void readSensor() {
  if (!sensorConnected) return;
  
  irValue = particleSensor.getIR();
  redValue = particleSensor.getRed();
  fingerDetected = (irValue > FINGER_THRESHOLD);
  
  if (!fingerDetected) {
    beatAvg = 0;
    beatsPerMinute = 0;
    spo2Valid = false;
    spo2 = 0;
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    rateSpot = 0;
    bufferIndex = 0;
    
    // Reset SpO2 calibration
    isCalibrating = true;
    calibrationSamples = 0;
    calibrationR = 0.0;
  }
}

void readMPU() {
  if (!mpuConnected) return;
  
  sensors_event_t a, g, temp;
  if (!mpu.getEvent(&a, &g, &temp)) {
    Serial.println("MPU read failed!");
    mpuConnected = false;
    return;
  }
  
  accelX = a.acceleration.x;
  accelY = a.acceleration.y;
  accelZ = a.acceleration.z;
  gyroX = g.gyro.x;
  gyroY = g.gyro.y;
  gyroZ = g.gyro.z;
}

void readMPUDirect() {
  if (!mpuConnected || mpuAddress == 0) return;
  
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B);
  byte error = Wire.endTransmission(false);
  
  if (error != 0) {
    Serial.println("MPU read error!");
    mpuConnected = false;
    return;
  }
  
  Wire.requestFrom(mpuAddress, (uint8_t)6);
  if (Wire.available() < 6) {
    return;
  }
  
  // Read raw accelerometer data (signed 16-bit)
  int16_t ax_raw = Wire.read() << 8 | Wire.read();
  int16_t ay_raw = Wire.read() << 8 | Wire.read();
  int16_t az_raw = Wire.read() << 8 | Wire.read();
  
  // Convert to m/s² (sensitivity for ±2g range: 16384 LSB/g)
  accelX = ax_raw / 16384.0 * 9.81;
  accelY = ay_raw / 16384.0 * 9.81;
  accelZ = az_raw / 16384.0 * 9.81;
}

void detectMovement() {
  float avgMovement = 0;
  int validSamples = 0;
  
  // Calculate movement from recent history
  for (int i = 1; i < 10; i++) {
    int idx1 = (accelHistoryIndex - i + 20) % 20;
    int idx2 = (accelHistoryIndex - i - 1 + 20) % 20;
    
    if (idx1 >= 0 && idx2 >= 0 && idx1 < 20 && idx2 < 20) {
      avgMovement += abs(accelHistory[idx1] - accelHistory[idx2]);
      validSamples++;
    }
  }
  
  if (validSamples > 0) {
    avgMovement /= validSamples;
  }
  
  // Apply sensitivity multiplier to movement detection
  isMoving = (avgMovement > (MOVEMENT_THRESHOLD / accelSensitivity));
}

void detectStep() {
  stepBuffer[stepBufferIndex] = totalAccel;
  stepBufferIndex = (stepBufferIndex + 1) % 10;
  
  float smoothedAccel = getSmoothedAccel();
  
  unsigned long currentTime = millis();
  
  // Dynamic thresholds based on current activity
  float dynamicHigh = stepThresholdHigh;
  float dynamicLow = stepThresholdLow;
  
  if (isMoving) {
    // Lower thresholds when moving for better sensitivity
    dynamicHigh = stepThresholdHigh * 0.9;
    dynamicLow = stepThresholdLow * 0.9;
  }
  
  if (smoothedAccel > dynamicHigh && !stepStateHigh) {
    stepStateHigh = true;
    
    if (currentTime - lastStepDetected > STEP_DEBOUNCE_MS) {
      stepCount++;
      lastStepDetected = currentTime;
      lastStepTime = currentTime;
      
      Serial.print("Step detected! Total: ");
      Serial.println(stepCount);
    }
  } 
  else if (smoothedAccel < dynamicLow && stepStateHigh) {
    stepStateHigh = false;
  }
  
  // Auto-calibrate thresholds every 5 seconds
  static unsigned long lastCalibration = 0;
  if (currentTime - lastCalibration > 5000) {
    calibrateStepThresholds();
    lastCalibration = currentTime;
  }
}

float getSmoothedAccel() {
  float sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += stepBuffer[i];
  }
  return sum / 10.0;
}

void calibrateStepThresholds() {
  if (!isMoving) return;
  
  float minVal = 999, maxVal = -999;
  for (int i = 0; i < 20; i++) {
    if (accelHistory[i] < minVal) minVal = accelHistory[i];
    if (accelHistory[i] > maxVal) maxVal = accelHistory[i];
  }
  
  if (maxVal - minVal > 1.5) {
    float range = maxVal - minVal;
    stepThresholdHigh = minVal + (range * 0.6);
    stepThresholdLow = minVal + (range * 0.4);
    
    Serial.print("Step thresholds calibrated: High=");
    Serial.print(stepThresholdHigh);
    Serial.print(", Low=");
    Serial.println(stepThresholdLow);
  }
}

void detectBeat() {
  irBuffer[bufferIndex] = irValue;
  bufferIndex = (bufferIndex + 1) % 100;
  
  // Find min and max in the buffer
  irMax = 0;
  irMin = 999999;
  long sum = 0;
  int count = 0;
  
  for (int i = 0; i < 100; i++) {
    if (irBuffer[i] > 0) {
      sum += irBuffer[i];
      count++;
      if (irBuffer[i] > irMax) irMax = irBuffer[i];
      if (irBuffer[i] < irMin) irMin = irBuffer[i];
    }
  }
  
  if (count > 0) {
    irAverage = sum / count;
  }
  
  long threshold = (irMax + irMin) / 2;
  static long lastIR = 0;
  static bool wasBelow = true;
  
  if (irValue > threshold && lastIR <= threshold && wasBelow) {
    unsigned long now = millis();
    unsigned long delta = now - lastBeatTime;
    
    if (delta > 300 && delta < 2000) {
      beatsPerMinute = 60000 / delta;
      
      if (beatsPerMinute >= MIN_BPM && beatsPerMinute <= MAX_BPM) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        
        int sum = 0, count = 0;
        for (byte i = 0; i < RATE_SIZE; i++) {
          if (rates[i] > 0) {
            sum += rates[i];
            count++;
          }
        }
        
        if (count > 0) {
          beatAvg = sum / count;
        }
      }
    }
    
    lastBeatTime = now;
    wasBelow = false;
  }
  
  if (irValue < threshold) {
    wasBelow = true;
  }
  
  lastIR = irValue;
}

void calculateSpO2() {
  if (!fingerDetected || irValue < FINGER_THRESHOLD) {
    spo2Valid = false;
    return;
  }
  
  // Simple SpO2 calculation using ratio of ratios method
  static float lastRed = 0;
  static float lastIR = 0;
  
  // Apply DC filtering
  redDc = DC_FILTER_ALPHA * redDc + (1.0 - DC_FILTER_ALPHA) * redValue;
  irDc = DC_FILTER_ALPHA * irDc + (1.0 - DC_FILTER_ALPHA) * irValue;
  
  // Calculate AC components
  redAc = redValue - redDc;
  irAc = irValue - irDc;
  
  // Avoid division by zero
  if (abs(irAc) > 10 && abs(redDc) > 1000 && abs(irDc) > 1000) {
    // Calculate ratio of ratios
    float redRatio = redAc / redDc;
    float irRatio = irAc / irDc;
    
    if (abs(irRatio) > 0.001) {
      rValue = redRatio / irRatio;
      
      // Simple calibration curve (adjust these coefficients based on your sensor)
      spo2 = 110.0 - 25.0 * rValue;
      
      // Clamp to reasonable values
      if (spo2 < SPO2_LOW_LIMIT) spo2 = SPO2_LOW_LIMIT;
      if (spo2 > SPO2_HIGH_LIMIT) spo2 = SPO2_HIGH_LIMIT;
      
      // Add to buffer for smoothing
      spo2Buffer[spo2BufferIndex] = spo2;
      spo2BufferIndex = (spo2BufferIndex + 1) % 10;
      
      // Calculate average
      float spo2Sum = 0;
      int validCount = 0;
      for (int i = 0; i < 10; i++) {
        if (spo2Buffer[i] > 0) {
          spo2Sum += spo2Buffer[i];
          validCount++;
        }
      }
      
      if (validCount > 5) {
        spo2 = spo2Sum / validCount;
        spo2Valid = true;
      }
    }
  }
  
  lastRed = redValue;
  lastIR = irValue;
}

void handleSerialCommands() {
  while (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.length() > 0) {
      if (command.startsWith("time:")) {
        // Parse time: "time:14,30,0" for 2:30:00 PM
        int comma1 = command.indexOf(',', 5);
        int comma2 = command.indexOf(',', comma1 + 1);
        
        if (comma1 > 0 && comma2 > comma1) {
          int hour = command.substring(5, comma1).toInt();
          int minute = command.substring(comma1 + 1, comma2).toInt();
          int second = command.substring(comma2 + 1).toInt();
          
          if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
            currentHour = hour;
            currentMinute = minute;
            currentSecond = second;
            lastMillis = millis();
            Serial.print("Time set to: ");
            Serial.println(formatTime(currentHour, currentMinute, currentSecond));
            
            updateClockDisplay();
          } else {
            Serial.println("Invalid time format. Use: time:HH,MM,SS (24-hour format)");
          }
        } else {
          Serial.println("Invalid time format. Use: time:HH,MM,SS (24-hour format)");
        }
      }
      else {
        char cmd = command.charAt(0);
        cmd = toLowerCase(cmd);
        
        switch(cmd) {
          case 'h':
            Serial.println("\n=== CONTROLS ===");
            Serial.println("BOOT Button - Switch screen (HR/Activity/SpO2)");
            Serial.println("Serial Commands:");
            Serial.println("  h - Show help");
            Serial.println("  r - Reset system");
            Serial.println("  z - Reset step counter to 0");
            Serial.println("  + - Add 10 steps");
            Serial.println("  d - Toggle debug mode");
            Serial.println("  time:HH,MM,SS - Set time (e.g., time:14,30,0)");
            Serial.println("  s+ - Increase MPU sensitivity");
            Serial.println("  s- - Decrease MPU sensitivity");
            Serial.println("========================\n");
            break;
            
          case 'r':
            resetAll();
            Serial.println("System reset");
            break;
            
          case 'z':
            stepCount = 0;
            Serial.println("Step counter reset to 0");
            break;
            
          case '+':
            stepCount += 10;
            Serial.print("Added 10 steps. Total: ");
            Serial.println(stepCount);
            break;
            
          case 'd':
            debugPrintInterval = (debugPrintInterval == 2000) ? 500 : 2000;
            Serial.print("Debug mode: ");
            Serial.println(debugPrintInterval == 500 ? "FAST (500ms)" : "SLOW (2000ms)");
            break;
            
          case 's':
            if (command.length() > 1 && command.charAt(1) == '+') {
              accelSensitivity += 0.5;
              if (accelSensitivity > 5.0) accelSensitivity = 5.0;
              Serial.print("MPU sensitivity increased to: x");
              Serial.println(accelSensitivity);
              
              // Update display if on activity screen
              if (screenMode == 1) {
                tft.fillRect(100, 260, 30, 10, BG_MID);
                tft.setTextColor(SPO2_BLUE);
                tft.setCursor(100, 260);
                tft.print(accelSensitivity, 1);
              }
            } else if (command.length() > 1 && command.charAt(1) == '-') {
              accelSensitivity -= 0.5;
              if (accelSensitivity < 0.5) accelSensitivity = 0.5;
              Serial.print("MPU sensitivity decreased to: x");
              Serial.println(accelSensitivity);
              
              // Update display if on activity screen
              if (screenMode == 1) {
                tft.fillRect(100, 260, 30, 10, BG_MID);
                tft.setTextColor(SPO2_BLUE);
                tft.setCursor(100, 260);
                tft.print(accelSensitivity, 1);
              }
            }
            break;
            
          default:
            Serial.print("Unknown command: '");
            Serial.print(command);
            Serial.println("'. Press 'h' for help.");
            break;
        }
      }
    }
  }
}

void resetAll() {
  if (sensorConnected) {
    particleSensor.softReset();
    delay(100);
    
    // Reconfigure sensor
    byte ledBrightness = 0x1F;
    byte sampleAverage = 4;
    byte ledMode = 2;
    int sampleRate = 100;
    int pulseWidth = 411;
    int adcRange = 4096;
    
    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);
  }
  
  beatAvg = 0;
  beatsPerMinute = 0;
  spo2 = 0;
  spo2Valid = false;
  
  for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
  rateSpot = 0;
  
  stepCount = 0;
  stepStateHigh = false;
  
  stepThresholdHigh = 11.5;
  stepThresholdLow = 9.0;
  
  // Reset SpO2 buffers
  for (int i = 0; i < 10; i++) {
    spo2Buffer[i] = 0;
  }
  spo2BufferIndex = 0;
  
  if (mpuConnected && mpuAddress != 0) {
    wakeUpMPU(mpuAddress);
  }
  
  // Reset time
  currentHour = 12;
  currentMinute = 0;
  currentSecond = 0;
  lastMillis = millis();
  
  // Reset sensor readings
  fingerDetected = false;
  irValue = 0;
  redValue = 0;
  
  // Redraw screen
  initializeScreen();
  
  Serial.println("System reset complete!");
}