/*
 * timecode.cpp
 * SMPTE Linear Timecode (LTC) decoder implementation
 * Copyright 2026 by DZED Systems LLC
 *
 * LTC Signal Format:
 * - 80 bits per frame (Manchester encoded)
 * - Bit rate = FPS × 80 (e.g., 2400 bps for 30 fps)
 * - Audio frequency: ~1200-2400 Hz
 * - Voltage: typically -10 dBV to +4 dBV (needs level shifter to 3.3V)
 */

#include "timecode.h"
#include <Arduino.h>

// Global timecode state
TimecodeConfig timecodeConfig = {0};
TimecodeData currentTimecode = {0};
volatile uint32_t timecodeFrameCount = 0;

// LTC decoder state
static uint8_t ltcBitBuffer[10];     // 80 bits = 10 bytes
static uint8_t ltcBitPosition = 0;
static uint32_t ltcLastEdgeTime = 0;
static uint8_t ltcLastState = 0;
static uint16_t ltcBitPeriod = 0;    // Expected bit period in microseconds

/*
 * Initialize timecode system
 */
void timecodeInit() {
  timecodeConfig.enabled = 0;
  timecodeConfig.syncMode = TIMECODE_SYNC_OFF;
  timecodeConfig.frameRate = TIMECODE_FPS_30;
  timecodeConfig.analogPin = A0;  // Default to A0
  timecodeConfig.threshold = 512; // Mid-range threshold

  // Clear timecode data
  currentTimecode.hours = 0;
  currentTimecode.minutes = 0;
  currentTimecode.seconds = 0;
  currentTimecode.frames = 0;
  currentTimecode.frameRate = TIMECODE_FPS_30;
  currentTimecode.dropFrame = 0;
  currentTimecode.valid = 0;
  currentTimecode.totalFrames = 0;

  timecodeFrameCount = 0;
  ltcBitPosition = 0;

  // Calculate expected bit period for 30 fps
  // 30 fps × 80 bits = 2400 bps = 417 μs per bit
  ltcBitPeriod = 417;
}

/*
 * Set timecode configuration
 */
void timecodeSetConfig(uint8_t frameRate, uint8_t syncMode) {
  timecodeConfig.frameRate = frameRate;
  timecodeConfig.syncMode = syncMode;
  timecodeConfig.enabled = (syncMode != TIMECODE_SYNC_OFF) ? 1 : 0;

  // Calculate bit period based on frame rate
  // Bit rate = FPS × 80 bits/frame
  switch (frameRate) {
    case TIMECODE_FPS_24:
      ltcBitPeriod = 521; // 24 × 80 = 1920 bps → 521 μs
      break;
    case TIMECODE_FPS_25:
      ltcBitPeriod = 500; // 25 × 80 = 2000 bps → 500 μs
      break;
    case TIMECODE_FPS_29_97:
    case TIMECODE_FPS_30:
    default:
      ltcBitPeriod = 417; // 30 × 80 = 2400 bps → 417 μs
      break;
  }

  // Reset decoder state
  ltcBitPosition = 0;
  currentTimecode.valid = 0;
}

/*
 * Get current timecode status
 */
void timecodeGetStatus(TimecodeData* data) {
  if (data) {
    *data = currentTimecode;
  }
}

/*
 * Update timecode decoder (call frequently from main loop)
 *
 * Manchester encoding: Each bit is represented by a transition
 * - "0" = high-to-low transition in middle of bit period
 * - "1" = low-to-high transition in middle of bit period
 */
void timecodeUpdate() {
  if (!timecodeConfig.enabled) {
    return;
  }

  // Read analog input and compare to threshold
  int analogValue = analogRead(timecodeConfig.analogPin);
  uint8_t currentState = (analogValue > timecodeConfig.threshold) ? 1 : 0;

  // Detect edge transitions
  if (currentState != ltcLastState) {
    uint32_t currentTime = micros();
    uint32_t edgeDelta = currentTime - ltcLastEdgeTime;

    // Check if edge is within expected bit period (±30% tolerance)
    uint16_t minPeriod = ltcBitPeriod - (ltcBitPeriod * 30 / 100);
    uint16_t maxPeriod = ltcBitPeriod + (ltcBitPeriod * 30 / 100);

    if (edgeDelta >= minPeriod && edgeDelta <= maxPeriod) {
      // Valid bit transition detected
      // Manchester: rising edge = 1, falling edge = 0
      uint8_t bit = currentState;
      ltcDecodeBit(bit);
    }
    else if (edgeDelta > maxPeriod) {
      // Lost sync - reset decoder
      ltcBitPosition = 0;
      currentTimecode.valid = 0;
    }

    ltcLastEdgeTime = currentTime;
    ltcLastState = currentState;
  }
}

/*
 * Decode a single LTC bit
 */
void ltcDecodeBit(uint8_t bit) {
  // Store bit in buffer
  uint8_t byteIndex = ltcBitPosition / 8;
  uint8_t bitIndex = ltcBitPosition % 8;

  if (byteIndex < 10) {
    if (bit) {
      ltcBitBuffer[byteIndex] |= (1 << bitIndex);
    } else {
      ltcBitBuffer[byteIndex] &= ~(1 << bitIndex);
    }

    ltcBitPosition++;

    // When we have 80 bits (10 bytes), process the frame
    if (ltcBitPosition >= 80) {
      ltcValidateFrame();
      ltcBitPosition = 0;
    }
  }
}

/*
 * Validate and decode complete LTC frame
 *
 * SMPTE LTC bit layout (80 bits total):
 * Bits 0-3:   Frame units (0-9)
 * Bits 4-7:   User bits 1
 * Bits 8-9:   Frame tens (0-2)
 * Bit 10:     Drop frame flag
 * Bit 11:     Color frame flag
 * Bits 12-15: User bits 2
 * Bits 16-19: Seconds units (0-9)
 * Bits 20-23: User bits 3
 * Bits 24-26: Seconds tens (0-5)
 * Bit 27:     Even parity / Phase correction
 * Bits 28-31: User bits 4
 * Bits 32-35: Minutes units (0-9)
 * Bits 36-39: User bits 5
 * Bits 40-42: Minutes tens (0-5)
 * Bit 43:     Binary group flag 1
 * Bits 44-47: User bits 6
 * Bits 48-51: Hours units (0-9)
 * Bits 52-55: User bits 7
 * Bits 56-57: Hours tens (0-2)
 * Bit 58:     Reserved
 * Bit 59:     Binary group flag 2
 * Bits 60-63: User bits 8
 * Bits 64-79: Sync word (0011 1111 1111 1101 = 0x3FFD)
 */
void ltcValidateFrame() {
  // Check sync word (bits 64-79 should be 0x3FFD)
  uint16_t syncWord = (ltcBitBuffer[8] | (ltcBitBuffer[9] << 8));
  if (syncWord != 0x3FFD) {
    // Invalid sync word - not a valid timecode frame
    currentTimecode.valid = 0;
    return;
  }

  // Extract timecode values
  uint8_t frameUnits = ltcBitBuffer[0] & 0x0F;
  uint8_t frameTens = (ltcBitBuffer[1] >> 0) & 0x03;
  uint8_t dropFrame = (ltcBitBuffer[1] >> 2) & 0x01;

  uint8_t secondsUnits = (ltcBitBuffer[2] >> 0) & 0x0F;
  uint8_t secondsTens = (ltcBitBuffer[3] >> 0) & 0x07;

  uint8_t minutesUnits = (ltcBitBuffer[4] >> 0) & 0x0F;
  uint8_t minutesTens = (ltcBitBuffer[5] >> 0) & 0x07;

  uint8_t hoursUnits = (ltcBitBuffer[6] >> 0) & 0x0F;
  uint8_t hoursTens = (ltcBitBuffer[7] >> 0) & 0x03;

  // Validate ranges
  if (frameUnits > 9 || frameTens > 2) {
    currentTimecode.valid = 0;
    return;
  }
  if (secondsUnits > 9 || secondsTens > 5) {
    currentTimecode.valid = 0;
    return;
  }
  if (minutesUnits > 9 || minutesTens > 5) {
    currentTimecode.valid = 0;
    return;
  }
  if (hoursUnits > 9 || hoursTens > 2) {
    currentTimecode.valid = 0;
    return;
  }

  // Decode BCD values
  currentTimecode.frames = frameTens * 10 + frameUnits;
  currentTimecode.seconds = secondsTens * 10 + secondsUnits;
  currentTimecode.minutes = minutesTens * 10 + minutesUnits;
  currentTimecode.hours = hoursTens * 10 + hoursUnits;
  currentTimecode.dropFrame = dropFrame;
  currentTimecode.frameRate = timecodeConfig.frameRate;
  currentTimecode.valid = 1;

  // Calculate total frame count
  currentTimecode.totalFrames = timecodeToFrameNumber(&currentTimecode);
  timecodeFrameCount = currentTimecode.totalFrames;
}

/*
 * Convert timecode to absolute frame number
 */
uint32_t timecodeToFrameNumber(TimecodeData* tc) {
  if (!tc) return 0;

  // Get frames per second
  uint8_t fps;
  switch (tc->frameRate) {
    case TIMECODE_FPS_24: fps = 24; break;
    case TIMECODE_FPS_25: fps = 25; break;
    case TIMECODE_FPS_29_97: fps = 30; break; // Approximate
    case TIMECODE_FPS_30:
    default: fps = 30; break;
  }

  // Calculate total frames (ignoring drop-frame for now)
  uint32_t totalFrames = 0;
  totalFrames += tc->hours * 3600 * fps;
  totalFrames += tc->minutes * 60 * fps;
  totalFrames += tc->seconds * fps;
  totalFrames += tc->frames;

  // TODO: Implement drop-frame compensation for 29.97 fps

  return totalFrames;
}

/*
 * Convert absolute frame number to timecode
 */
void frameNumberToTimecode(uint32_t frameNum, uint8_t frameRate, TimecodeData* tc) {
  if (!tc) return;

  // Get frames per second
  uint8_t fps;
  switch (frameRate) {
    case TIMECODE_FPS_24: fps = 24; break;
    case TIMECODE_FPS_25: fps = 25; break;
    case TIMECODE_FPS_29_97: fps = 30; break;
    case TIMECODE_FPS_30:
    default: fps = 30; break;
  }

  // Calculate timecode components
  tc->frames = frameNum % fps;
  frameNum /= fps;

  tc->seconds = frameNum % 60;
  frameNum /= 60;

  tc->minutes = frameNum % 60;
  frameNum /= 60;

  tc->hours = frameNum % 24;

  tc->frameRate = frameRate;
  tc->dropFrame = 0;
  tc->valid = 1;
}
