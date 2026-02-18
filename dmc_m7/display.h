/*
 * display.h
 * dmc-lite OLED display support
 *
 * Provides real-time status display for DMC motion control system
 * Compatible with SSD1306 128x64 I2C OLED displays
 *
 * Features:
 * - System status and version information
 * - Motor position and status (up to 32 motors)
 * - Connection status
 * - Limit switch status
 * - Camera trigger status
 * - Error messages and alerts
 * - Menu navigation for settings
 *
 * Hardware Requirements:
 * - SSD1306 128x64 OLED display (I2C)
 * - Connect to Arduino Giga R1 I2C pins (SDA/SCL)
 * - Standard I2C address: 0x3C or 0x3D
 *
 * Library Requirements:
 * - Adafruit_SSD1306
 * - Adafruit_GFX
 *
 * Created for dmc-lite v1.8.0
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

// Display modes/screens
enum DisplayMode {
  DISPLAY_MODE_STATUS,      // Main status screen
  DISPLAY_MODE_MOTORS,      // Motor positions and status
  DISPLAY_MODE_LIMITS,      // Limit switch status
  DISPLAY_MODE_ERRORS,      // Error messages
  DISPLAY_MODE_INFO         // System information
};

// Display update flags
#define DISPLAY_UPDATE_STATUS    0x01
#define DISPLAY_UPDATE_MOTORS    0x02
#define DISPLAY_UPDATE_LIMITS    0x04
#define DISPLAY_UPDATE_ERROR     0x08
#define DISPLAY_UPDATE_FORCE     0x80

// Initialize the display
// Returns true if successful, false if display not found
bool display_init();

// Update the display with current system state
// Should be called periodically (e.g., 10-20 Hz)
void display_update();

// Set the current display mode
void display_set_mode(DisplayMode mode);

// Get the current display mode
DisplayMode display_get_mode();

// Show an error message on the display
void display_show_error(const char* errorMsg);

// Clear any error message
void display_clear_error();

// Update motor count and frame count (for status display)
void display_set_motor_info(uint8_t motorCount, uint32_t frameCount);

// Update connection status
void display_set_connected(bool connected);

// Update move state
void display_set_move_state(int32_t state);

// Force a full display refresh
void display_refresh();

// Turn display on/off (power saving)
void display_set_power(bool on);

// Check if display is initialized
bool display_is_initialized();

#endif /* DISPLAY_H_ */
