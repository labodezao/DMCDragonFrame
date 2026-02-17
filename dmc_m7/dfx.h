/*
 * dfx.h
 * dmc-lite source code
 * Copyright 2023 by DZED Systems LLC
 */

#ifndef DFX_H_
#define DFX_H_

#include <stdint.h>

#define DMC_VERSION_MAJOR 1
#define DMC_VERSION_MINOR 3
#define DMC_VERSION_REV 0

#define MOTOR_COUNT 16
#define MOTOR_CAM_COUNT 9
#define GIO_OUTPUTS 2
#define GIO_INPUTS 1u

#define P2P_MOVE_COUNT 8

#define FRAME_COUNT 10000

#define MOVE_LOAD_NONE 0
#define MOVE_LOAD_FRAME 1

#define MOVE_STATE_JOG 0
#define MOVE_STATE_SHOOT_PREROLL 10
#define MOVE_STATE_SHOOT_WAIT 11
#define MOVE_STATE_SHOOT 12
#define MOVE_STATE_ALL_JOG 100

struct MotorMove
{
  float time;
  float position;
  float velocity;
  float acceleration;
};

struct Motor
{
  float maxVelocity;
  float maxAcceleration;
  float position;
  uint8_t moving;
  uint8_t wasMoving;
  uint8_t config;
  uint8_t limitLowEnabled;
  int32_t limitLow;
  uint8_t limitHighEnabled;
  int32_t limitHigh;

  uint8_t stopping;

  int32_t currentMove;
  float currentMoveTime;

  float currentVelocity;

  MotorMove moves[P2P_MOVE_COUNT];
};

struct AxisMoveData
{
  int32_t frameCount;
  int32_t position[FRAME_COUNT];
};

enum GoMotionState
{
  GO_MO_INACTIVE,
  GO_MO_PREROLL,
  GO_MO_WAIT,
  GO_MO_PRE_ACCEL,
  GO_MO_ACCEL,
  GO_MO_MOVE,
  GO_MO_DECEL,
  GO_MO_POST_DECEL
};

enum GoMotionMode
{
  GO_MO_MODE_SHOOTING,
  GO_MO_MODE_SHOOTING2,
  GO_MO_MODE_RUN_LIVE
};

struct GoMotionMove
{
  GoMotionMode mode;
  GoMotionState state;
  uint16_t flags;

  float time;        // time within state
  float runningTime; // total time

  float shutterOpenTime;
  float shutterCloseTime;

  float preAccelDuration;

  // accel portion
  float accelPosition[MOTOR_COUNT];
  float acceleration[MOTOR_COUNT];
  float accelDuration;

  // main move
  float moveStartTime;
  float moveEndTime;
  float moveDuration;
  float moveTimeSegment;

  float moveP0[MOTOR_COUNT];
  float moveP1[MOTOR_COUNT];
  float moveV[MOTOR_COUNT];

  // decel portion
  float decelPosition[MOTOR_COUNT];
  float decelVelocity[MOTOR_COUNT];
  float deceleration[MOTOR_COUNT];
  float decelDuration;

  float postDecelDuration;
};

struct GoMotionOverride
{
  int32_t enabled;
  int32_t posA;
  int32_t posB;
};

#define BIT_SET(v, b) (v |= (1 << (b)))
#define BIT_CLEAR(v, b) (v &= ~(1 << (b)))

#define BOUND(a, b, c) ((b) < (a) ? (a) : ((b) > (c) ? (c) : (b)))

#define CAMERA_OFF 0x0
#define CAMERA_SHUTTER 0x1
#define CAMERA_METER 0x2

#define GET_MOTOR_DIR(m) ((motorDirection & (1U << m)) != 0)

#define IN_RANGE(v, mn, mx) ((mn) <= (v) && (v) <= (mx))

// flags for delayed messages
#define DMC_MSG_FLAG_RT_GO 0x0001
#define DMC_MSG_FLAG_RT_END 0x0002
#define DMC_MSG_FLAG_MOTOR_HARD_STOP 0x0004

#define GET_MOTOR_POSITION(m) ( sharedData->accum[m] >> 32U )
#define SET_MOTOR_POSITION(m, p)                                                                                       \
  {                                                                                                                    \
    sharedData->accum[m] = uint64_t(p) << 32U;                                                                         \
  }

#define FRAME_TO_POSITION(frame) ((int32_t)((frame)*100000.0f))
#define POSITION_TO_FRAME(pos) ((float)(pos)*0.00001f)

#define DATA_RATE 50
#define DATA_RATE_TIME_SEGMENT (0.02f)       // (1 / DATA_RATE)


#endif /* DFX_H_ */
