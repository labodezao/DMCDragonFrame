/*
 * encoder.h
 * Rotary encoder support for manual motor control
 * Copyright 2026 by DZED Systems LLC
 *
 * Supports quadrature encoders for:
 * - Manual jog control
 * - Frame-by-frame positioning
 * - Focus pulling
 * - Stop-motion animation
 */

#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdint.h>

#define MAX_ENCODERS 8  // Maximum number of encoders supported

// Encoder configuration
struct EncoderConfig {
  uint8_t enabled;        // Encoder enabled
  uint8_t pinA;           // Phase A pin
  uint8_t pinB;           // Phase B pin
  uint8_t motorIndex;     // Motor this encoder controls (0-31)
  int16_t scaleFactor;    // Steps per encoder count (can be negative for reverse)
  uint8_t mode;           // 0=direct position, 1=jog speed
};

// Encoder state
struct EncoderState {
  volatile int32_t position;     // Current encoder position
  volatile int32_t lastPosition; // Last reported position
  uint8_t lastStateA;            // Last A pin state
  uint8_t lastStateB;            // Last B pin state
};

// Global encoder arrays
extern EncoderConfig encoderConfigs[MAX_ENCODERS];
extern EncoderState encoderStates[MAX_ENCODERS];

// Function prototypes
void encoderInit();
void encoderSetConfig(uint8_t encoderIndex, uint8_t pinA, uint8_t pinB, uint8_t motorIndex, int16_t scaleFactor);
void encoderReset(uint8_t encoderIndex);
int32_t encoderGetPosition(uint8_t encoderIndex);
int32_t encoderGetDelta(uint8_t encoderIndex);  // Get change since last call
void encoderUpdate();  // Call from main loop

// Interrupt handlers (called by pin change interrupts)
void encoderISR(uint8_t encoderIndex);

#endif /* ENCODER_H_ */
