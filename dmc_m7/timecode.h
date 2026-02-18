/*
 * timecode.h
 * SMPTE Linear Timecode (LTC) decoder for dmc-lite
 * Copyright 2026 by DZED Systems LLC
 *
 * Supports SMPTE timecode standards:
 * - 24 fps (Film)
 * - 25 fps (PAL)
 * - 30 fps (NTSC Non-Drop)
 * - 29.97 fps (NTSC Drop-Frame)
 */

#ifndef TIMECODE_H_
#define TIMECODE_H_

#include <stdint.h>

// Timecode frame rates
#define TIMECODE_FPS_24    0
#define TIMECODE_FPS_25    1
#define TIMECODE_FPS_30    2
#define TIMECODE_FPS_29_97 3

// Timecode sync modes
#define TIMECODE_SYNC_OFF      0  // Timecode reading disabled
#define TIMECODE_SYNC_READ     1  // Read timecode but don't sync
#define TIMECODE_SYNC_CHASE    2  // Chase timecode (sync playback)
#define TIMECODE_SYNC_JAM      3  // Jam sync (lock once, then free-run)

// Timecode structure (SMPTE format: HH:MM:SS:FF)
struct TimecodeData {
  uint8_t hours;          // 0-23
  uint8_t minutes;        // 0-59
  uint8_t seconds;        // 0-59
  uint8_t frames;         // 0-29 (depends on frame rate)
  uint8_t frameRate;      // TIMECODE_FPS_* constant
  uint8_t dropFrame;      // 1 if drop-frame, 0 if non-drop
  uint8_t valid;          // 1 if timecode is valid, 0 if invalid
  uint32_t totalFrames;   // Total frame count from 00:00:00:00
};

// Timecode configuration
struct TimecodeConfig {
  uint8_t enabled;        // Timecode input enabled
  uint8_t syncMode;       // TIMECODE_SYNC_* mode
  uint8_t frameRate;      // Expected frame rate
  uint8_t analogPin;      // Analog input pin for LTC signal
  uint16_t threshold;     // Signal threshold for bit detection (0-1023)
};

// Global timecode state
extern TimecodeConfig timecodeConfig;
extern TimecodeData currentTimecode;
extern volatile uint32_t timecodeFrameCount;

// Function prototypes
void timecodeInit();
void timecodeUpdate();  // Call from main loop at high frequency
void timecodeSetConfig(uint8_t frameRate, uint8_t syncMode);
void timecodeGetStatus(TimecodeData* data);
uint32_t timecodeToFrameNumber(TimecodeData* tc);
void frameNumberToTimecode(uint32_t frameNum, uint8_t frameRate, TimecodeData* tc);

// LTC decoder internal functions
void ltcDecodeBit(uint8_t bit);
void ltcProcessByte(uint8_t byte, uint8_t position);
void ltcValidateFrame();

#endif /* TIMECODE_H_ */
