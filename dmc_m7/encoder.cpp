/*
 * encoder.cpp
 * Rotary encoder implementation with quadrature decoding
 * Copyright 2026 by DZED Systems LLC
 *
 * Quadrature Encoding:
 * Phase A and B create a pattern that indicates direction:
 * - CW rotation:  A leads B (00 → 10 → 11 → 01 → 00)
 * - CCW rotation: B leads A (00 → 01 → 11 → 10 → 00)
 */

#include "encoder.h"
#include <Arduino.h>

// Global encoder arrays
EncoderConfig encoderConfigs[MAX_ENCODERS] = {0};
EncoderState encoderStates[MAX_ENCODERS] = {0};

/*
 * Initialize encoder system
 */
void encoderInit() {
  for (int i = 0; i < MAX_ENCODERS; i++) {
    encoderConfigs[i].enabled = 0;
    encoderConfigs[i].pinA = 0xFF;
    encoderConfigs[i].pinB = 0xFF;
    encoderConfigs[i].motorIndex = 0;
    encoderConfigs[i].scaleFactor = 1;
    encoderConfigs[i].mode = 0;

    encoderStates[i].position = 0;
    encoderStates[i].lastPosition = 0;
    encoderStates[i].lastStateA = 0;
    encoderStates[i].lastStateB = 0;
  }
}

/*
 * Configure an encoder
 */
void encoderSetConfig(uint8_t encoderIndex, uint8_t pinA, uint8_t pinB, uint8_t motorIndex, int16_t scaleFactor) {
  if (encoderIndex >= MAX_ENCODERS) return;

  EncoderConfig* cfg = &encoderConfigs[encoderIndex];

  // Disable before reconfiguring
  cfg->enabled = 0;

  // Set new configuration
  cfg->pinA = pinA;
  cfg->pinB = pinB;
  cfg->motorIndex = motorIndex;
  cfg->scaleFactor = scaleFactor;

  // Configure pins
  if (pinA != 0xFF && pinB != 0xFF) {
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);

    // Read initial state
    encoderStates[encoderIndex].lastStateA = digitalRead(pinA);
    encoderStates[encoderIndex].lastStateB = digitalRead(pinB);

    // Enable encoder
    cfg->enabled = 1;
  }
}

/*
 * Reset encoder position to zero
 */
void encoderReset(uint8_t encoderIndex) {
  if (encoderIndex >= MAX_ENCODERS) return;

  encoderStates[encoderIndex].position = 0;
  encoderStates[encoderIndex].lastPosition = 0;
}

/*
 * Get absolute encoder position
 */
int32_t encoderGetPosition(uint8_t encoderIndex) {
  if (encoderIndex >= MAX_ENCODERS) return 0;

  return encoderStates[encoderIndex].position;
}

/*
 * Get encoder position change since last call
 */
int32_t encoderGetDelta(uint8_t encoderIndex) {
  if (encoderIndex >= MAX_ENCODERS) return 0;

  EncoderState* state = &encoderStates[encoderIndex];
  int32_t delta = state->position - state->lastPosition;
  state->lastPosition = state->position;

  return delta;
}

/*
 * Update encoder readings (call from main loop)
 */
void encoderUpdate() {
  for (uint8_t i = 0; i < MAX_ENCODERS; i++) {
    if (encoderConfigs[i].enabled) {
      encoderISR(i);
    }
  }
}

/*
 * Encoder interrupt service routine
 * Decodes quadrature signals to determine direction
 *
 * State transition table:
 * Previous (AB) | Current (AB) | Direction
 * --------------|--------------|----------
 * 00            | 01           | CCW
 * 00            | 10           | CW
 * 01            | 00           | CW
 * 01            | 11           | CCW
 * 10            | 00           | CCW
 * 10            | 11           | CW
 * 11            | 01           | CW
 * 11            | 10           | CCW
 */
void encoderISR(uint8_t encoderIndex) {
  if (encoderIndex >= MAX_ENCODERS) return;
  if (!encoderConfigs[encoderIndex].enabled) return;

  EncoderConfig* cfg = &encoderConfigs[encoderIndex];
  EncoderState* state = &encoderStates[encoderIndex];

  // Read current pin states
  uint8_t stateA = digitalRead(cfg->pinA);
  uint8_t stateB = digitalRead(cfg->pinB);

  // Check if state changed
  if (stateA != state->lastStateA || stateB != state->lastStateB) {
    // Create 4-bit value from old and new states
    uint8_t combined = (state->lastStateA << 3) | (state->lastStateB << 2) | (stateA << 1) | stateB;

    // Decode direction from state transition
    // Using lookup table for speed
    static const int8_t PROGMEM transitionTable[16] = {
      0,   // 0000: no change
      -1,  // 0001: CCW (00 → 01)
      1,   // 0010: CW (00 → 10)
      0,   // 0011: invalid
      1,   // 0100: CW (01 → 00)
      0,   // 0101: no change
      0,   // 0110: invalid
      -1,  // 0111: CCW (01 → 11)
      -1,  // 1000: CCW (10 → 00)
      0,   // 1001: invalid
      0,   // 1010: no change
      1,   // 1011: CW (10 → 11)
      0,   // 1100: invalid
      1,   // 1101: CW (11 → 01)
      -1,  // 1110: CCW (11 → 10)
      0    // 1111: no change
    };

    int8_t direction = transitionTable[combined];
    state->position += direction;

    // Update last states
    state->lastStateA = stateA;
    state->lastStateB = stateB;
  }
}
