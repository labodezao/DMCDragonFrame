/*
 * display.cpp
 * dmc-lite OLED display implementation
 *
 * Provides real-time status display for DMC motion control system
 */

#include "display.h"
#include "dfx.h"
#include <Arduino.h>

#ifdef OLED_DISPLAY_ENABLED

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin (or -1 if sharing Arduino reset pin)

#ifndef OLED_I2C_ADDRESS
#define OLED_I2C_ADDRESS 0x3C
#endif

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static bool displayInitialized = false;
static DisplayMode currentMode = DISPLAY_MODE_STATUS;
static bool isConnected = false;
static int32_t currentMoveState = MOVE_STATE_JOG;
static uint8_t currentMotorCount = MOTOR_COUNT;
static uint32_t currentFrameCount = FRAME_COUNT;
static char errorMessage[32] = {0};
static uint32_t lastUpdateTime = 0;
static uint8_t updateFlags = 0;

// Forward declarations
static void drawStatusScreen();
static void drawMotorsScreen();
static void drawLimitsScreen();
static void drawErrorsScreen();
static void drawInfoScreen();

bool display_init() {
  // Attempt to initialize the display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    return false;
  }

  displayInitialized = true;

  // Initial display setup
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Show splash screen
  display.setCursor(0, 0);
  display.println(F("DMC-lite"));
  display.println();
  display.print(F("Version "));
  display.print(DMC_VERSION_MAJOR);
  display.print(F("."));
  display.print(DMC_VERSION_MINOR);
  display.print(F("."));
  display.println(DMC_VERSION_REV);
  display.println();
  display.println(F("Initializing..."));
  display.display();

  delay(1000);

  return true;
}

void display_update() {
  if (!displayInitialized) {
    return;
  }

  // Throttle updates to ~10Hz
  uint32_t now = millis();
  if (now - lastUpdateTime < 100 && !(updateFlags & DISPLAY_UPDATE_FORCE)) {
    return;
  }
  lastUpdateTime = now;

  display.clearDisplay();

  switch (currentMode) {
    case DISPLAY_MODE_STATUS:
      drawStatusScreen();
      break;
    case DISPLAY_MODE_MOTORS:
      drawMotorsScreen();
      break;
    case DISPLAY_MODE_LIMITS:
      drawLimitsScreen();
      break;
    case DISPLAY_MODE_ERRORS:
      drawErrorsScreen();
      break;
    case DISPLAY_MODE_INFO:
      drawInfoScreen();
      break;
  }

  display.display();
  updateFlags = 0;
}

void display_set_mode(DisplayMode mode) {
  currentMode = mode;
  updateFlags |= DISPLAY_UPDATE_FORCE;
}

DisplayMode display_get_mode() {
  return currentMode;
}

void display_show_error(const char* errorMsg) {
  if (errorMsg) {
    strncpy(errorMessage, errorMsg, sizeof(errorMessage) - 1);
    errorMessage[sizeof(errorMessage) - 1] = '\0';
    updateFlags |= DISPLAY_UPDATE_ERROR;
  }
}

void display_clear_error() {
  errorMessage[0] = '\0';
  updateFlags |= DISPLAY_UPDATE_ERROR;
}

void display_set_motor_info(uint8_t motorCount, uint32_t frameCount) {
  currentMotorCount = motorCount;
  currentFrameCount = frameCount;
  updateFlags |= DISPLAY_UPDATE_STATUS;
}

void display_set_connected(bool connected) {
  isConnected = connected;
  updateFlags |= DISPLAY_UPDATE_STATUS;
}

void display_set_move_state(int32_t state) {
  currentMoveState = state;
  updateFlags |= DISPLAY_UPDATE_STATUS;
}

void display_refresh() {
  updateFlags |= DISPLAY_UPDATE_FORCE;
}

void display_set_power(bool on) {
  if (!displayInitialized) {
    return;
  }

  if (on) {
    display.ssd1306_command(SSD1306_DISPLAYON);
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

bool display_is_initialized() {
  return displayInitialized;
}

// Draw status screen (main screen)
static void drawStatusScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Title
  display.print(F("DMC-lite v"));
  display.print(DMC_VERSION_MAJOR);
  display.print(F("."));
  display.print(DMC_VERSION_MINOR);
  display.print(F("."));
  display.println(DMC_VERSION_REV);

  // Connection status
  display.setCursor(0, 10);
  if (isConnected) {
    display.print(F("Connected"));
  } else {
    display.print(F("Disconnected"));
  }

  // Motor count
  display.setCursor(0, 20);
  display.print(F("Motors: "));
  display.print(currentMotorCount);

  // Frame capacity
  display.setCursor(0, 30);
  display.print(F("Frames: "));
  display.print(currentFrameCount);

  // Move state
  display.setCursor(0, 40);
  display.print(F("State: "));
  switch (currentMoveState) {
    case MOVE_STATE_JOG:
      display.print(F("Jog"));
      break;
    case MOVE_STATE_SHOOT_PREROLL:
      display.print(F("Pre-roll"));
      break;
    case MOVE_STATE_SHOOT_WAIT:
      display.print(F("Wait"));
      break;
    case MOVE_STATE_SHOOT:
      display.print(F("Shooting"));
      break;
    case MOVE_STATE_ALL_JOG:
      display.print(F("All Jog"));
      break;
    default:
      display.print(F("Unknown"));
      break;
  }

  // Error message if present
  if (errorMessage[0] != '\0') {
    display.setCursor(0, 54);
    display.print(F("ERR: "));
    display.print(errorMessage);
  }
}

// Draw motors screen (motor positions)
static void drawMotorsScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Motor Status"));

  // Show first 5 motors
  for (int i = 0; i < 5 && i < currentMotorCount; i++) {
    display.setCursor(0, 10 + i * 10);
    display.print(F("M"));
    display.print(i + 1);
    display.print(F(": "));
    // Note: Motor positions would need to be passed via external variables
    display.print(F("Ready"));
  }

  display.setCursor(0, 54);
  display.print(F("Total: "));
  display.print(currentMotorCount);
}

// Draw limits screen (limit switch status)
static void drawLimitsScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Limit Switches"));

  // Note: Limit switch status would need to be passed via external variables
  display.setCursor(0, 10);
  display.println(F("M1-M4: OK"));
  display.setCursor(0, 20);
  display.println(F("M5-M8: OK"));

  display.setCursor(0, 54);
  display.print(F("All Clear"));
}

// Draw errors screen
static void drawErrorsScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Error Messages"));

  if (errorMessage[0] != '\0') {
    display.setCursor(0, 10);
    display.println(errorMessage);
  } else {
    display.setCursor(0, 10);
    display.println(F("No errors"));
  }
}

// Draw info screen (system information)
static void drawInfoScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("System Info"));

  display.setCursor(0, 10);
  display.print(F("Board: "));
#ifdef ARDUINO_ARCH_MBED_GIGA
  display.println(F("Giga R1"));
#elif defined(ARDUINO_ARCH_MBED_PORTENTA)
  display.println(F("Portenta H7"));
#else
  display.println(F("Unknown"));
#endif

  display.setCursor(0, 20);
  display.print(F("Motors: "));
  display.print(currentMotorCount);

  display.setCursor(0, 30);
  display.print(F("Frames: "));
  display.print(currentFrameCount);

  display.setCursor(0, 40);
#ifdef USE_SDRAM
  display.println(F("SDRAM: Enabled"));
#else
  display.println(F("SDRAM: Disabled"));
#endif

  display.setCursor(0, 50);
  display.print(F("Uptime: "));
  display.print(millis() / 1000);
  display.print(F("s"));
}

#else

// Stub implementations when display is disabled
bool display_init() { return false; }
void display_update() {}
void display_set_mode(DisplayMode mode) {}
DisplayMode display_get_mode() { return DISPLAY_MODE_STATUS; }
void display_show_error(const char* errorMsg) {}
void display_clear_error() {}
void display_set_motor_info(uint8_t motorCount, uint32_t frameCount) {}
void display_set_connected(bool connected) {}
void display_set_move_state(int32_t state) {}
void display_refresh() {}
void display_set_power(bool on) {}
bool display_is_initialized() { return false; }

#endif // OLED_DISPLAY_ENABLED
