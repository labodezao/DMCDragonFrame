/*
 * dmc_m7.ino
 * dmc-lite source code
 * Copyright 2023 by DZED Systems LLC
 *
 * Target core: Main Core
 * Flash split: 1.5MB M7 + 0.5MB M4
 */

#include "config.h"
#include "dfx.h"
#include "dmc_msg.h"
#include "motion.h"

#include <RPC.h>

#ifdef CORE_CM4
#error "Make sure to target the Main core with flash split 1.5MB M7 + 0.5MB M4"
#endif

int killSwitchState;

void sendHello(uint32_t id);

uint32_t _triggers;
void setTriggers(uint32_t triggers);

RingBufferN<1024> messageBuffer;
uint32_t messageQueue; // queued messages

/*
 * Program state information
 */
static int32_t moveState;

/*
 * Message state machine variables.
 */
static int32_t loadMoveState;

/*
 * Uploaded move data
 */
static AxisMoveData move[MOTOR_COUNT];
static int32_t moveStartFrame;
static int32_t moveFrameCount;
static int32_t movePositionFrame;
uint16_t hardLimits = 0;

static uint8_t syncTriggers;
static uint8_t triggerMask;
static uint8_t triggerData[FRAME_COUNT];

/*
 * Motor state information.
 */
static Motor motors[MOTOR_CAM_COUNT]; // extra motor is camera

static uint8_t motorsSendPosition;
static uint32_t motorDirection;

/*
 * HardStop globals
 */
static uint32_t hardStopCounter;
static uint32_t stopAllLastTime;
static uint8_t exceptionCode;

/*
 * Jog/move globals
 */
static Motor frameTimeMotor;
static uint8_t frameTimeMotorDir;
static uint8_t frameTimeStopCounter;
static uint8_t playBlipStarted;
static uint8_t playBlipActive;
static uint8_t playBlipDuration;
static uint32_t playBlipLocation;   // 0x01 logic out, 0x02 relay out, 0x04 dmx channel
static uint16_t playBlipDmxChannel; // channel for DMX blip

static uint8_t cameraTriggerCountdown;

static uint16_t leds;

static GoMotionMove goMotionMove;
static GoMotionOverride goMotionOverride[MOTOR_COUNT];

static uint16_t usbLedCounter;

uint8_t triggerOutState;

uint32_t lastPositionTime = 0;
uint8_t hardStop = 0;
uint8_t limitStopMotor = 0;
uint8_t limitStopMotorForward = 0;
int8_t switchInput;
int8_t switchInputCounter = 0;
uint8_t motorsMoving = 0;

void initMotor(Motor *m);
int32_t updateMotorVelocity(Motor *m, float timeSegment);
void clearAxisMove(AxisMoveData *axis);
void stopAll(uint8_t emergency);
void jogMotor(Motor *m, int32_t target, int32_t motorIndex);
void setMovePositionFrame(int32_t frame);
void positionFrame(int32_t frame);
void go(uint8_t *motorsMoving);
int32_t updateMotorVelocities();
int32_t stopMotor(Motor *motor, int32_t motorIndex, int8_t emergency);
void sendMotorPositions();
void clearGomoMove(GoMotionMove *move);
void setCamera(uint8_t cameraValue);
void setMotorDir(int32_t motorIndex, float dir);
int32_t getMotorDir(int32_t m);
void writeOutputMessage();
void transmitMessages();

uint8_t motorDataLoaded = 0;

struct DmcSharedData
{
  volatile uint32_t motorDataLoaded;
  volatile int64_t nextSpeed[MOTOR_CAM_COUNT];
  volatile int64_t accum[MOTOR_CAM_COUNT];
  volatile uint32_t motorDirection;
  volatile uint8_t cameraValue;
  volatile uint16_t cameraOpenAngle;
  volatile uint16_t cameraCloseAngle;
};

DmcSharedData *sharedData;
uintptr_t sharedDataPointer()
{
  return (uintptr_t)sharedData;
}

int8_t logicSwitchInput()
{
#ifdef LOGIC_SWITCH_PIN
  return !digitalRead(LOGIC_SWITCH_PIN);
#else
  return 0;
#endif
}

void setCamera(uint8_t val)
{
  sharedData->cameraValue = val;
}

void setup()
{
  int m;

  sharedData = (DmcSharedData *)0x3800fd00;
  memset(sharedData, 0, sizeof(DmcSharedData));

  Serial.begin(115200);

  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);

#ifdef LOGIC_SWITCH_PIN
  pinMode(LOGIC_SWITCH_PIN, INPUT_PULLUP);
#endif

  digitalWrite(LEDR, HIGH);
  digitalWrite(LEDG, HIGH);
  digitalWrite(LEDB, HIGH);

  for (int i = 0; i < 3; ++i)
  {
    digitalWrite(LEDB, LOW);
    delay(200);
    digitalWrite(LEDB, HIGH);
    delay(200);
  }

#ifdef KILL_SWITCH_PIN
  pinMode(KILL_SWITCH_PIN, INPUT_PULLUP);
  killSwitchState = !digitalRead(KILL_SWITCH_PIN);
#else
  killSwitchState = 0;
#endif

  RPC.begin();

  RPC.bind("sharedDataPointer", sharedDataPointer);

  sharedData->motorDataLoaded = 0;
  sharedData->motorDirection = 0;
  for (m = 0; m < MOTOR_CAM_COUNT; ++m)
  {
    sharedData->nextSpeed[m] = 0;
  }

  stopAllLastTime = 0;
  exceptionCode = 0;
  motorsSendPosition = 0;
  moveState = MOVE_STATE_JOG;
  syncTriggers = 0;

  pinMode(LOGIC_OUT_0, OUTPUT);
  digitalWrite(LOGIC_OUT_0, LOW);
  pinMode(LOGIC_OUT_1, OUTPUT);
  digitalWrite(LOGIC_OUT_1, LOW);

  triggerOutState = (1 << 2);

  dmc_msg_init();
  messageQueue = 0;

  loadMoveState = MOVE_LOAD_NONE;

  // initialize move structure
  moveStartFrame = 1;
  moveFrameCount = 0;
  movePositionFrame = -1;
  for (m = 0; m < MOTOR_COUNT; ++m)
  {
    clearAxisMove(&move[m]);
  }

  memset(triggerData, 0, FRAME_COUNT);

  // initialize motor structures
  for (m = 0; m < MOTOR_CAM_COUNT; ++m)
  {
    initMotor(&motors[m]);

    sharedData->nextSpeed[m] = 0;
    setMotorDir(m, 1);

    SET_MOTOR_POSITION(m, 0);
  }
  motors[MOTOR_COUNT].config = DMC_MOTOR_CONFIG_ENABLED;
  motors[MOTOR_COUNT].maxAcceleration = 40000;
  motors[MOTOR_COUNT].maxVelocity = 40000;

  initMotor(&frameTimeMotor);
  frameTimeStopCounter = 0;
  playBlipStarted = 0;
  playBlipActive = 0;
  playBlipDuration = 0;
  playBlipLocation = 0;
  playBlipDmxChannel = 0;
  cameraTriggerCountdown = 0;

  clearGomoMove(&goMotionMove);

  setCamera(CAMERA_OFF);
  setTriggers(0);

  leds = 0;

  // send hello message on startup
  sendHello(0);

  switchInput = logicSwitchInput();
}

void loop()
{

  int32_t updatedVelocities = updateMotorVelocities();
  uint16_t cmd = 0;

  uint8_t switchToggled = 0;
  int m;
  int i;
  Motor *motorPtr;
  int eStopOn = 0;

  transmitMessages();

  if (updatedVelocities)
  {
    if (usbLedCounter)
    {
      --usbLedCounter;
    }

#ifdef KILL_SWITCH_PIN
    eStopOn = !digitalRead(KILL_SWITCH_PIN);
    if (eStopOn != killSwitchState)
    {
      killSwitchState = eStopOn;
      digitalWrite(LEDR, killSwitchState ? LOW : HIGH);
    }
#endif

    if (logicSwitchInput())
    {
      ++switchInputCounter;
      if (switchInputCounter > 5)
      {
        switchInputCounter = 5;
        if (!switchInput)
        {
          switchInput = 1;
          switchToggled = 1;
        }
      }
    }
    else
    {
      --switchInputCounter;
      if (switchInputCounter < -5)
      {
        switchInputCounter = -5;
        if (switchInput)
        {
          switchInput = 0;
          switchToggled = 1;
        }
      }
    }

    if (switchToggled)
    {
      dmc_msg_prepare(DMC_MSG_GIO_IN, 0);
      dmc_msg_out_dword(switchInput);
      writeOutputMessage();
    }
    if (!hardStop)
    {
      if (motorsMoving && eStopOn) // add other conditions
      {
        hardStopCounter = 0;
        hardStop = 1;
        stopAll(1);

        messageQueue |= DMC_MSG_FLAG_MOTOR_HARD_STOP;
      }
    }
    else
    {
      hardStopCounter++;
      if (!motorsMoving && hardStopCounter > 100)
      {
        hardStop = 0;

        if (limitStopMotor)
        {
          limitStopMotor = 0;
        }
      }
    }

    motorsMoving = 0;

    for (m = 0; m < MOTOR_COUNT; ++m)
    {
      motorPtr = &motors[m];
      if (motorPtr->moving)
      {
        motorsMoving = 1;
        break;
      }
    }

    if (moveState == MOVE_STATE_ALL_JOG && frameTimeMotor.moving)
    {
      motorsMoving = 1;
    }

    if (playBlipActive)
    {
      --playBlipActive;

      if (playBlipLocation & 0x01)
      {
        if (playBlipActive)
          BIT_SET(_triggers, 0);
        else
          BIT_CLEAR(_triggers, 0);
      }
      if (playBlipLocation & 0x02)
      {
        if (playBlipActive)
          BIT_SET(_triggers, 1);
        else
          BIT_CLEAR(_triggers, 1);
      }
      setTriggers(_triggers);
    }
  }

  if ((usbLedCounter / 10) % 2)
  {
    digitalWrite(LEDB, LOW);
  }
  else
  {
    digitalWrite(LEDB, HIGH);
  }

  unsigned long msTime = millis();

  // send motor positions
  if (updatedVelocities)
  {
    if (lastPositionTime > msTime)
      lastPositionTime = 0; // rollover
    if ((motorsSendPosition && (moveState != MOVE_STATE_JOG || msTime - lastPositionTime > 100)) ||
        (motorsMoving && (msTime - lastPositionTime > 50)))
    {
      sendMotorPositions();
      motorsSendPosition = 0;
    }

    if (moveState == MOVE_STATE_ALL_JOG && !motorsMoving && goMotionMove.state == GO_MO_PREROLL)
    {
      moveState = MOVE_STATE_SHOOT_PREROLL;
      for (m = 0; m < MOTOR_COUNT; ++m)
      {
        motorPtr = &motors[m];
        if ((motorPtr->config & DMC_MOTOR_CONFIG_ENABLED) && !(motorPtr->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
        {
          calculatePointToPoint(motorPtr, (int32_t)goMotionMove.accelPosition[m], 0.0f);
        }
      }
      motorsMoving = 1;
    }
    else if (moveState == MOVE_STATE_SHOOT_PREROLL && !motorsMoving)
    {
      moveState = MOVE_STATE_SHOOT_WAIT;
      setTriggers(0);
    }

    if (moveState == MOVE_STATE_SHOOT_WAIT && switchToggled)
    {
      go(&motorsMoving);
      messageQueue |= DMC_MSG_FLAG_RT_GO;
    }

    if (cameraTriggerCountdown)
    {
      --cameraTriggerCountdown;
      if (!cameraTriggerCountdown)
      {
        setCamera(CAMERA_OFF);
      }
    }
  }

  {
    uint8_t len = 64;
    while (len && Serial.available())
    {

      int readData = Serial.read();

      --len;

      uint8_t csumValid = 0;
      uint32_t msgId = 0;
      uint16_t msgLength = 0;
      cmd = dmc_msg_process_character(readData, &csumValid, &msgId, &msgLength);
      if (cmd)
      {

        int32_t responseCode = DMC_ACK_OK;

        if (!csumValid)
        {
          responseCode = DMC_ACK_ERR_CHECKSUM;
        }
        else if (hardStop && cmd != DMC_MSG_MOTOR_GET_POSITION && cmd >= DMC_MSG_MOTOR_MOVE)
        {
          responseCode = DMC_ACK_ERR_HARD_LOW;
        }
        else
        {
          if (cmd == DMC_MSG_HI)
          {
            responseCode = 0;
            sendHello(msgId);
          }
          else if (cmd == DMC_MSG_GIO_OUT)
          {
            uint32_t gio = dmc_msg_read_dword();
            setTriggers(gio);
          }
          else if (cmd == DMC_MSG_GIO_CAM)
          {
            uint32_t cam = dmc_msg_read_dword();
            setCamera(cam);
          }
          else if (cmd == DMC_MSG_MOTOR_MOVE)
          {
            if (msgLength != 5)
            {
              responseCode = DMC_ACK_ERR_GENERAL;
            }
            else
            {
              int32_t motor = dmc_msg_read_byte();

              if (!IN_RANGE(motor, 1, MOTOR_COUNT))
              {
                responseCode = DMC_ACK_ERR_RANGE;
              }
              else
              {
                int32_t position = dmc_msg_read_dword();
                --motor;

                motorPtr = &motors[motor];
                if (moveState != MOVE_STATE_JOG && motorsMoving && !(motorPtr->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
                {
                  responseCode = DMC_ACK_ERR_MOVING;
                }
                else
                {
                  if (position != motorPtr->position || motorPtr->moving)
                  {
                    calculatePointToPoint(&motors[motor], position, 0.0f);
                    moveState = MOVE_STATE_JOG;
                    syncTriggers = 0;
                  }
                  else
                  {
                    sendMotorPositions();
                  }
                  if (!(motorPtr->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
                  {
                    movePositionFrame = -1;
                  }
                }
              }
            }
          }
          else if (cmd == DMC_MSG_MOTOR_RESET_POSITION)
          {
            if (motorsMoving)
            {
              responseCode = DMC_ACK_ERR_MOVING;
            }
            else if (msgLength != 5)
            {
              responseCode = DMC_ACK_ERR_GENERAL;
            }
            else
            {
              int32_t motor = dmc_msg_read_byte();

              if (!IN_RANGE(motor, 1, MOTOR_COUNT))
              {
                responseCode = DMC_ACK_ERR_RANGE;
              }
              else
              {
                --motor;
                int32_t pos = dmc_msg_read_dword();
                motors[motor].position = pos;
                SET_MOTOR_POSITION(motor, pos);
                sendMotorPositions();
              }
            }
          }
          else if (cmd == DMC_MSG_MOTOR_STATUS)
          {
            uint32_t status = 0;

            for (i = 0; i < MOTOR_COUNT; ++i)
            {
              if (motors[i].moving)
                BIT_SET(status, i);
            }

            responseCode = 0;

            dmc_msg_prepare(DMC_MSG_MOTOR_STATUS, msgId);
            dmc_msg_out_dword(status);
            dmc_msg_out_byte(0); // DMX status
            writeOutputMessage();
          }
          else if (cmd == DMC_MSG_MOTOR_STOP)
          {
            int32_t motor = dmc_msg_read_byte();

            if (!IN_RANGE(motor, 1, MOTOR_COUNT))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else
            {
              --motor;
              if (moveState == MOVE_STATE_JOG ||
                  (cmd == DMC_MSG_MOTOR_STOP && (motors[motor].config & DMC_MOTOR_CONFIG_LIVE_CONTROL)))
              {
                stopMotor(&motors[motor], motor, 0);
                if (!(motors[motor].config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
                {
                  moveState = MOVE_STATE_JOG;
                  syncTriggers = 0;
                }
              }
            }
          }
          else if (cmd == DMC_MSG_MOTOR_STOP_ALL)
          {
            uint32_t flags = 0;
            if (!dmc_msg_read_at_end())
              flags = dmc_msg_read_dword();
            if (motorsMoving && !flags)
            {
              hardStopCounter = 0;
              hardStop = 1;
            }
            stopAll(0);
          }
          else if (cmd == DMC_MSG_MOTOR_JOG)
          {
            int32_t motor = dmc_msg_read_byte();

            if (!IN_RANGE(motor, 1, MOTOR_COUNT))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else
            {
              --motor;

              motorPtr = &motors[motor];
              if (moveState != MOVE_STATE_JOG && motorsMoving && !(motorPtr->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
              {
                responseCode = DMC_ACK_ERR_MOVING;
              }
              else
              {
                uint16_t speed = dmc_msg_read_word();
                int32_t destination = dmc_msg_read_dword();

                {
                  float maxVelocity = motorPtr->maxVelocity;
                  float maxAcceleration = motorPtr->maxAcceleration;
                  float accelSeconds = maxVelocity / maxAcceleration;

                  float speedAdjustment = speed * 0.0001f;
                  motorPtr->maxVelocity = fmaxf(4.0f, (motorPtr->maxVelocity * speedAdjustment));
                  motorPtr->maxAcceleration = fmaxf(4.0f, (motorPtr->maxAcceleration * speedAdjustment));
                  motorPtr->maxAcceleration =
                    fmaxf((float)motorPtr->maxAcceleration, fabsf(motorPtr->currentVelocity) / accelSeconds);

                  jogMotor(motorPtr, destination, motor);

                  motorPtr->maxVelocity = maxVelocity;
                  motorPtr->maxAcceleration = maxAcceleration;
                }

                if (!(motorPtr->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
                {
                  moveState = MOVE_STATE_JOG;
                  movePositionFrame = -1;
                  syncTriggers = 0;
                }
              }
            }
          }
          else if (cmd == DMC_MSG_MOTOR_SET_SPEED)
          {
            int32_t motor = dmc_msg_read_byte();
            if (!IN_RANGE(motor, 1, MOTOR_COUNT))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else
            {
              --motor;
              motorPtr = &motors[motor];

              motorPtr->maxVelocity = fmaxf(10.0f, (float)dmc_msg_read_dword());
              motorPtr->maxAcceleration = dmc_msg_read_dword();
              if (motorPtr->maxAcceleration == 0)
              {
                motorPtr->maxAcceleration = motorPtr->maxVelocity * 0.5f;
              }
            }
          }
          else if (cmd == DMC_MSG_MOTOR_CONFIGURE)
          {
            int32_t motor = dmc_msg_read_byte();
            if (!IN_RANGE(motor, 1, MOTOR_COUNT))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else
            {
              --motor;
              motors[motor].config = dmc_msg_read_byte();
            }
          }
          else if (cmd == DMC_MSG_MOTOR_SET_LIMITS)
          {
            int32_t motor = dmc_msg_read_byte();
            if (!IN_RANGE(motor, 1, MOTOR_COUNT))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else
            {
              --motor;
              motorPtr = &motors[motor];

              motorPtr->limitLowEnabled = dmc_msg_read_byte();
              motorPtr->limitLow = dmc_msg_read_dword();
              motorPtr->limitHighEnabled = dmc_msg_read_byte();
              motorPtr->limitHigh = dmc_msg_read_dword();

              dmc_msg_read_byte(); // limit set
            }
          }
          else if (cmd == DMC_MSG_MOTOR_GET_POSITION)
          {
            sendMotorPositions();
          }
          else if (cmd == DMC_MSG_RT_UPLOAD_MOVE_BEGIN)
          {
            loadMoveState = MOVE_LOAD_FRAME;

            triggerMask = 0;
            memset(triggerData, 0, FRAME_COUNT);

            moveStartFrame = dmc_msg_read_dword();
            moveFrameCount = 1 + (dmc_msg_read_dword() - moveStartFrame);

            for (m = 0; m < MOTOR_COUNT; ++m)
            {
              clearAxisMove(&move[m]);
              move[m].frameCount = moveFrameCount;
              move[m].position[0] = 0;
              move[m].position[1] = 0;
            }
          }
          else if (cmd == DMC_MSG_RT_UPLOAD_MOVE_AXIS)
          {
            int32_t motor = dmc_msg_read_byte();
            uint32_t index = dmc_msg_read_dword();
            uint32_t finalData = (index & DMC_DMX_FLAG_FINAL_SET);
            index &= ~DMC_DMX_FLAG_FINAL_SET;

            if (!IN_RANGE(motor, 1, MOTOR_COUNT) || !IN_RANGE((int32_t)index, 0, moveFrameCount))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else
            {
              --motor;
              index++;

              int32_t *positions = move[motor].position;
              while (!dmc_msg_read_at_end())
              {
                positions[index++] = dmc_msg_read_dword();
                if ((int32_t)index > moveFrameCount)
                  break;
              }
              if (finalData)
              {
                const int32_t finalPos = positions[index - 1];
                while ((int32_t)index <= moveFrameCount)
                  positions[index++] = finalPos;
              }
            }
          }
          else if (cmd == DMC_MSG_RT_UPLOAD_MOVE_TRIGGERS)
          {
            triggerMask = dmc_msg_read_dword();

            while (!dmc_msg_read_at_end())
            {
              uint32_t index = dmc_msg_read_dword();
              uint32_t value = dmc_msg_read_dword();

              if ((int32_t)index < moveFrameCount)
              {
                triggerData[index + 1] = value; // let's use 1-based to match everything else
              }
            }
          }
          else if (cmd == DMC_MSG_RT_UPLOAD_MOVE_END)
          {
            if (loadMoveState == MOVE_LOAD_FRAME)
            {
              for (m = 0; m < MOTOR_COUNT; ++m)
              {
                int32_t dx = move[m].position[2] - move[m].position[1];
                move[m].position[0] = move[m].position[1] - dx; // calculate frame 0 (before move start)
              }
            }
            loadMoveState = MOVE_LOAD_NONE;
          }
          else if (cmd == DMC_MSG_RT_POSITION_FRAME)
          {
            int32_t frame = dmc_msg_read_dword();

            playBlipLocation = 0;

            if (moveState == MOVE_STATE_SHOOT)
            {
              responseCode = DMC_ACK_ERR_MOVING;
            }
            else if (moveFrameCount)
            {
              positionFrame(frame);
            }
            else
            {
              responseCode = DMC_ACK_ERR_GENERAL;
            }
          }
          else if (cmd == DMC_MSG_RT_RUN_MOVE)
          {
            if (motorsMoving && moveState != MOVE_STATE_ALL_JOG)
            {
              responseCode = DMC_ACK_ERR_MOVING;
            }
            else if (movePositionFrame == -1)
            {
              responseCode = DMC_ACK_ERR_NOT_IN_POSITION;
            }
            else
            {
              clearGomoMove(&goMotionMove);

              float fps = dmc_msg_read_dword() * 0.001f;
              int32_t startFrame = dmc_msg_read_dword();
              int32_t endFrame = dmc_msg_read_dword();

              float preRollTime = dmc_msg_read_dword() * 0.001f;
              float postRollTime = dmc_msg_read_dword() * 0.001f;

              syncTriggers = dmc_msg_read_byte() ? 1 : 0;

              playBlipLocation = dmc_msg_read_dword();
              playBlipActive = 0;
              playBlipStarted = 0;

              playBlipDmxChannel = dmc_msg_read_word();
              playBlipDuration = dmc_msg_read_word() / 5;
              if (playBlipDuration == 0)
                playBlipDuration = 20;

              goMotionMove.flags = dmc_msg_read_word();
              if (goMotionMove.flags & (DMC_RT_PLAYBACK_PING_PONG | DMC_RT_PLAYBACK_LOOP))
              {
                postRollTime = 0;
              }

              if (goMotionMove.flags & DMC_RT_CAMERA_STILLS)
              {
                sharedData->cameraOpenAngle = dmc_msg_read_word();
                sharedData->cameraCloseAngle = dmc_msg_read_word();
                if (sharedData->cameraOpenAngle >= sharedData->cameraCloseAngle)
                {
                  sharedData->cameraOpenAngle = 1024;
                  sharedData->cameraCloseAngle = 2048;
                }
                else
                {
                  sharedData->cameraOpenAngle = (sharedData->cameraOpenAngle * 4095L) / 360;
                  sharedData->cameraCloseAngle = (sharedData->cameraCloseAngle * 4095L) / 360;
                }
              }
              else
              {
                sharedData->cameraOpenAngle = 0;
                sharedData->cameraCloseAngle = 0;
              }

              calculateRunLive(motors, move, &goMotionMove, startFrame, endFrame, fps, preRollTime, postRollTime);

              for (m = 0; m < MOTOR_COUNT; ++m)
              {
                if ((motors[m].config & DMC_MOTOR_CONFIG_ENABLED) &&
                    goMotionMove.accelPosition[m] != motors[m].position)
                {
                  if ((motors[m].limitLowEnabled && goMotionMove.accelPosition[m] < motors[m].limitLow) ||
                      (motors[m].limitHighEnabled && goMotionMove.accelPosition[m] > motors[m].limitHigh))
                  {
                    responseCode = DMC_ACK_ERR_PREROLL;
                    break;
                  }
                  int32_t finalPos =
                    (int32_t)(goMotionMove.decelPosition[m] + goMotionMove.decelVelocity[m] * (postRollTime * 0.5f));
                  if ((motors[m].limitLowEnabled && finalPos < motors[m].limitLow) ||
                      (motors[m].limitHighEnabled && finalPos > motors[m].limitHigh))
                  {
                    responseCode = DMC_ACK_ERR_POSTROLL;
                    break;
                  }
                }
              }

              if (responseCode == DMC_ACK_OK)
              {
                // jog all motors back to start frame
                if (moveState != MOVE_STATE_ALL_JOG)
                {
                  frameTimeMotor.position =
                    FRAME_TO_POSITION(movePositionFrame); // only reset frame position if we've been jogging
                }
                frameTimeMotor.maxAcceleration = FRAME_TO_POSITION(4);
                frameTimeMotor.maxVelocity = FRAME_TO_POSITION(fps);

                moveState = MOVE_STATE_ALL_JOG;

                if (startFrame < moveStartFrame)
                  startFrame = moveStartFrame;
                if (startFrame > moveStartFrame + moveFrameCount - 1)
                  startFrame = moveStartFrame + moveFrameCount - 1;

                frameTimeStopCounter = 0;
                calculatePointToPoint(&frameTimeMotor, FRAME_TO_POSITION(startFrame), 0.0f);

                motorsMoving = 1;
              }
            }
          }
          else if (cmd == DMC_MSG_RT_SHOOT_FRAME)
          {
            memset(goMotionOverride, 0, sizeof(GoMotionOverride) * MOTOR_COUNT);

            // sf [frame] [dir] [exposure time ms] [blur rate]
            int32_t frame = dmc_msg_read_dword();
            if (!IN_RANGE(frame, moveStartFrame, moveStartFrame + moveFrameCount - 1))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else if (motorsMoving)
            {
              responseCode = DMC_ACK_ERR_MOVING;
            }
            else
            {
              frame = 1 + (frame - moveStartFrame);

              int32_t dir = dmc_msg_read_byte();
              int32_t exposure = dmc_msg_read_dword();
              int32_t blur = dmc_msg_read_word();

              clearGomoMove(&goMotionMove);

              if (blur == 0)
              {
                blur = 1000;
              }

              while (!dmc_msg_read_at_end())
              {
                int32_t channel = dmc_msg_read_byte() - 1;
                goMotionOverride[channel].enabled = 1;
                int32_t posA = dmc_msg_read_dword();
                int32_t posB = dmc_msg_read_dword();
                goMotionOverride[channel].posA = posA;
                goMotionOverride[channel].posB = posB;
              }

              if (dir == 0)
                dir = -1;

              calculateGoMotionMove(motors, move, &goMotionMove, frame, dir, exposure, blur, goMotionOverride);

              moveState = MOVE_STATE_SHOOT_PREROLL;
              for (m = 0; m < MOTOR_COUNT; ++m)
              {
                if ((motors[m].config & DMC_MOTOR_CONFIG_ENABLED) &&
                    !(motors[m].config & DMC_MOTOR_CONFIG_LIVE_CONTROL) && (motors[m].config & DMC_MOTOR_CONFIG_BLUR) &&
                    goMotionMove.accelPosition[m] != motors[m].position)
                {
                  calculatePointToPoint(&motors[m], (int32_t)goMotionMove.accelPosition[m], 0.5f);
                }
              }
            }
          }
          else if (cmd == DMC_MSG_RT_SHOOT_FRAME2)
          {
            memset(goMotionOverride, 0, sizeof(GoMotionOverride) * MOTOR_COUNT);

            int32_t frame = dmc_msg_read_dword();
            if (!IN_RANGE(frame, moveStartFrame, moveStartFrame + moveFrameCount - 1))
            {
              responseCode = DMC_ACK_ERR_RANGE;
            }
            else if (motorsMoving)
            {
              responseCode = DMC_ACK_ERR_MOVING;
            }
            else
            {
              int32_t exposure = dmc_msg_read_dword();
              int16_t shutterOpen = dmc_msg_read_word();
              int16_t shutterClose = dmc_msg_read_word();

              clearGomoMove(&goMotionMove);

              if (exposure == 0)
              {
                exposure = 1000;
              }

              while (!dmc_msg_read_at_end())
              {
                int32_t channel = dmc_msg_read_byte() - 1;
                int32_t posA = dmc_msg_read_dword();
                int32_t posB = dmc_msg_read_dword();
                if (channel >= 0 && channel < MOTOR_COUNT)
                {
                  goMotionOverride[channel].enabled = 1;
                  goMotionOverride[channel].posA = posA;
                  goMotionOverride[channel].posB = posB;
                }
              }

              calculateGoMotionMove2(motors, move, &goMotionMove, frame, exposure, shutterOpen, shutterClose,
                                     goMotionOverride);

              moveState = MOVE_STATE_SHOOT_PREROLL;
              for (m = 0; m < MOTOR_COUNT; ++m)
              {
                if ((motors[m].config & DMC_MOTOR_CONFIG_ENABLED) &&
                    !(motors[m].config & DMC_MOTOR_CONFIG_LIVE_CONTROL) && (motors[m].config & DMC_MOTOR_CONFIG_BLUR) &&
                    goMotionMove.accelPosition[m] != motors[m].position)
                {
                  calculatePointToPoint(&motors[m], (int32_t)goMotionMove.accelPosition[m], 0.0f);
                }
              }
            }
          }
          else if (cmd == DMC_MSG_RT_GO)
          {
            if (moveState == MOVE_STATE_SHOOT_WAIT)
            {
              go(&motorsMoving);
            }
            else
            {
              responseCode = DMC_ACK_ERR_NOT_IN_POSITION;
            }
          }
          else if (cmd == DMC_MSG_RT_STOP_LOOP)
          {
            goMotionMove.flags &= ~(DMC_RT_PLAYBACK_PING_PONG | DMC_RT_PLAYBACK_LOOP); // stop after next loop iteration
          }
          else if (cmd == DMC_MSG_RT_JOG_ALL)
          {
            if (motorsMoving && moveState != MOVE_STATE_ALL_JOG)
            {
              responseCode = DMC_ACK_ERR_MOVING;
            }
            else if (movePositionFrame == -1)
            {
              responseCode = DMC_ACK_ERR_NOT_IN_POSITION;
            }
            else
            {
              // setup frameTimeMotor to represent time
              if (moveState != MOVE_STATE_ALL_JOG)
              {
                frameTimeMotor.position =
                  FRAME_TO_POSITION(movePositionFrame); // only reset frame position if we've been jogging
              }
              frameTimeMotor.maxAcceleration = FRAME_TO_POSITION(4);
              frameTimeMotor.maxVelocity = FRAME_TO_POSITION(dmc_msg_read_dword() * 0.001f);

              moveState = MOVE_STATE_ALL_JOG;

              frameTimeStopCounter = 0;
              calculatePointToPoint(&frameTimeMotor, FRAME_TO_POSITION(dmc_msg_read_dword()), 0.0f);
            }
          }
          else // unsupported
          {
            responseCode = DMC_ACK_ERR_UNSUPPORTED;
          }
        }

        if (responseCode)
        {
          dmc_msg_prepare(cmd | DMC_MSG_FLAG_ACK, msgId);
          dmc_msg_out_dword(responseCode);
          writeOutputMessage();
        }

        break; // exit if command was processed
      }
    }

    if (len != 64)
    {
      if (!usbLedCounter)
      {
        usbLedCounter = 20;
      }
      else if (usbLedCounter < 10)
      {
        usbLedCounter += 20;
      }
    }
  }
}

void initMotor(Motor *m)
{
  memset(m, 0, sizeof(Motor));
  m->maxVelocity = 5000.0f;
  m->maxAcceleration = 20000.0f;
}

void clearGomoMove(GoMotionMove *move)
{
  frameTimeMotorDir = 1;
  memset(move, 0, sizeof(GoMotionMove));
}

void clearAxisMove(AxisMoveData *axisData)
{
  memset(axisData, 0, sizeof(AxisMoveData));
}

void jogMotor(Motor *motor, int32_t target, int32_t motorIndex)
{
  // ideally send motor to distance where decel happens after 2 seconds
  float maxVelocity = motor->maxVelocity;
  float maxAcceleration = motor->maxAcceleration;
  float vi = motor->currentVelocity;

  int32_t dir = (target > motor->position) ? 1 : -1;
  // if switching direction, just stop
  if (vi * dir < 0)
  {
    stopMotor(motor, motorIndex, 0);
    return;
  }
  if (fabsf(target - motor->position) < 0.001f)
  {
    return;
  }

  // given current velocity vi
  // compute distance so that decel starts after 0.5 seconds
  // time to accel
  // time at maxvelocity
  // time to decel
  float accelTime = 0, atMaxVelocityTime = 0;
  float decelTime = 0.5f;
  if (fabsf(vi) < maxVelocity)
  {
    accelTime = (maxVelocity - fabsf(vi)) / maxAcceleration;
    if (accelTime < decelTime)
    {
      atMaxVelocityTime = decelTime - accelTime;
    }
    else
    {
      accelTime = decelTime;
    }
  }
  else
  {
    atMaxVelocityTime = decelTime;
  }
  float maxVelocityReached = fabsf(vi) + maxAcceleration * accelTime;

  float delta = fabsf(vi) * accelTime + (0.5f * maxAcceleration * accelTime * accelTime) +
                atMaxVelocityTime * maxVelocityReached +
                0.5f * (maxVelocityReached * maxVelocityReached) / maxAcceleration; // = 0.5 * a * t^2 -> t = (v/a)

  int32_t dest = lround(motor->position + dir * delta);

  // now clamp to target
  if ((dir == 1 && dest > target) || (dir == -1 && dest < target))
  {
    dest = target;
  }

  calculatePointToPoint(motor, dest, 0.0f);
}

void setMovePositionFrame(int32_t frame)
{
  movePositionFrame = frame;

  if (syncTriggers)
  {
    int32_t index = BOUND(1, 1 + (frame - moveStartFrame), moveFrameCount);

    if (triggerMask)
    {
      uint8_t value = triggerData[index];

      uint32_t maskOut = triggerMask;
      maskOut = ~maskOut;

      setTriggers((_triggers & maskOut) | value);
    }
  }
}

void go(uint8_t *motorsMoving)
{
  int32_t m;

  if (moveState == MOVE_STATE_SHOOT_WAIT)
  {
    moveState = MOVE_STATE_SHOOT;
    goMotionMove.state = GO_MO_PRE_ACCEL;
    for (m = 0; m < MOTOR_COUNT; ++m)
    {
      if ((motors[m].config & DMC_MOTOR_CONFIG_ENABLED) && !(motors[m].config & DMC_MOTOR_CONFIG_LIVE_CONTROL) &&
          (goMotionMove.mode == GO_MO_MODE_RUN_LIVE || (motors[m].config & DMC_MOTOR_CONFIG_BLUR)) &&
          move[m].frameCount)
      {
        *motorsMoving = 1;
        motors[m].moving = 1;
      }
    }
    if (goMotionMove.mode == GO_MO_MODE_SHOOTING || goMotionMove.mode == GO_MO_MODE_SHOOTING2)
    {
      setCamera(CAMERA_METER);
    }
    else
    {
      if (goMotionMove.flags & DMC_RT_CAMERA_VIDEO)
      {
        setCamera(CAMERA_METER | CAMERA_SHUTTER);
        cameraTriggerCountdown = 5;
      }
      playBlipStarted = 0;
      playBlipActive = playBlipDuration;
    }
  }
}

void positionFrame(int32_t frame)
{
  int32_t m;

  int32_t frameIndex = BOUND(1, 1 + (frame - moveStartFrame), moveFrameCount);

  moveState = MOVE_STATE_JOG;

  for (m = 0; m < MOTOR_COUNT; ++m)
  {
    if ((motors[m].config & DMC_MOTOR_CONFIG_ENABLED) && !(motors[m].config & DMC_MOTOR_CONFIG_LIVE_CONTROL) &&
        move[m].frameCount)
    {
      int32_t moveFrame = frameIndex > move[m].frameCount ? move[m].frameCount : frameIndex;
      calculatePointToPoint(&motors[m], move[m].position[moveFrame], 0.5f);
    }
  }

  motors[MOTOR_COUNT].currentVelocity = 0;
  motors[MOTOR_COUNT].position = frame * 4096;
  SET_MOTOR_POSITION(MOTOR_COUNT, motors[MOTOR_COUNT].position);

  setMovePositionFrame(frame);
}

void sendHello(uint32_t id)
{
  dmc_msg_prepare(DMC_MSG_HI, id);

  int i = 0;

  dmc_msg_out_byte('d');
  ++i;
  dmc_msg_out_byte('m');
  ++i;
  dmc_msg_out_byte('c');
  ++i;
  dmc_msg_out_byte('-');
  ++i;
  dmc_msg_out_byte('l');
  ++i;
  dmc_msg_out_byte('i');
  ++i;
  dmc_msg_out_byte('t');
  ++i;
  dmc_msg_out_byte('e');
  ++i;

  for (; i < 32; ++i)
    dmc_msg_out_byte(0);

  dmc_msg_out_byte(DMC_VERSION_MAJOR);
  dmc_msg_out_byte(DMC_VERSION_MINOR);
  dmc_msg_out_byte(DMC_VERSION_REV);
  dmc_msg_out_byte(MOTOR_COUNT);
  dmc_msg_out_word(0);           // DMX channel count
  dmc_msg_out_byte(GIO_OUTPUTS); // GIO OUT count
  dmc_msg_out_byte(GIO_INPUTS);  // GIO IN count
  dmc_msg_out_byte(0);           // HW LIMIT SET count
  dmc_msg_out_dword(FRAME_COUNT);
  dmc_msg_out_dword(DMC_CAP_REAL_TIME | DMC_CAP_GO_MOTION | DMC_CAP_GO_MOTION2 | DMC_CAP_COUPLE_MOTORS |
                    DMC_CAP_REAL_TIME_LOOP | DMC_CAP_REAL_TIME_CAMERA); // capabilities

  writeOutputMessage();
}

void sendMotorPositions()
{
  lastPositionTime = millis();

  int i;

  dmc_msg_prepare(DMC_MSG_MOTOR_GET_POSITION, 0);
  dmc_msg_out_dword((uint32_t)(frameTimeMotor.position / 100));
  for (i = 0; i < MOTOR_COUNT; ++i)
    dmc_msg_out_dword((int32_t)(motors[i].position));
  writeOutputMessage();
}

void writeOutputMessage()
{
  uint16_t msgLength = 0;
  uint8_t *buf = dmc_msg_finalize_output(&msgLength);
  if (buf && msgLength)
  {
    if (msgLength < messageBuffer.availableForStore())
    {
      while (msgLength--)
      {
        messageBuffer.store_char(*buf++);
      }
    }
    // otherwise we are dropping the message. so sad!
  }
}

void transmitMessages()
{
  if ((messageQueue & DMC_MSG_FLAG_MOTOR_HARD_STOP) && messageBuffer.availableForStore() >= 24)
  {
    messageQueue &= ~DMC_MSG_FLAG_MOTOR_HARD_STOP;
    dmc_msg_prepare(DMC_MSG_MOTOR_HARD_STOP, 0);
    dmc_msg_out_byte(0);
    writeOutputMessage();
  }
  if ((messageQueue & DMC_MSG_FLAG_RT_GO) && messageBuffer.availableForStore() >= 24)
  {
    messageQueue &= ~DMC_MSG_FLAG_RT_GO;
    dmc_msg_prepare(DMC_MSG_RT_GO, 0);
    writeOutputMessage();
  }
  if ((messageQueue & DMC_MSG_FLAG_RT_END) && messageBuffer.availableForStore() >= 24)
  {
    messageQueue &= ~DMC_MSG_FLAG_RT_END;
    dmc_msg_prepare(DMC_MSG_RT_END, 0);
    writeOutputMessage();
  }

  uint16_t outMsgIndex = std::min(messageBuffer.available(), 32); // Serial.availableForWrite always returns 0

  if (!outMsgIndex)
    return;

  if (!usbLedCounter)
  {
    usbLedCounter = 20;
  }
  else if (usbLedCounter < 10)
  {
    usbLedCounter += 20;
  }

  while (outMsgIndex--)
  {
    uint32_t c = messageBuffer.read_char();
    Serial.write(c);
  }
}

void setTriggers(uint32_t triggers)
{
  digitalWrite(LOGIC_OUT_0, (triggers & 0x01) ? HIGH : LOW);
  digitalWrite(LOGIC_OUT_1, (triggers & 0x02) ? HIGH : LOW);
}

/*
 * Calculates the motor velocity and step count.
 */
int32_t updateMotorVelocities()
{
  if (sharedData->motorDataLoaded)
  {
    return 0;
  }

  int m = 0;
  float xn, dx;
  uint8_t shouldHardStop = 0;
  float t, tSqr;
  Motor *motor = 0;

  for (m = 0; m < MOTOR_COUNT; ++m)
  {
    motor = &motors[m];

    if (!motor->moving && motor->wasMoving)
    {
      float newPos = (float)(sharedData->accum[m] / double(0x100000000));
      if (!exceptionCode && fabsf(newPos - motor->position) > 10)
      {
        exceptionCode = 100;
        shouldHardStop = 1;

        dmc_msg_prepare(DMC_MSG_MOTOR_HARD_STOP, 0);
        dmc_msg_out_byte(exceptionCode);
        dmc_msg_out_byte(m + 1);
        writeOutputMessage();
      }
      motor->position = newPos;
    }

    motor->wasMoving = motor->moving;
    motor->currentVelocity = 0;
  }

  if (moveState == MOVE_STATE_JOG || moveState == MOVE_STATE_SHOOT_PREROLL)
  {
    for (m = 0; m < MOTOR_COUNT; ++m)
    {
      motor = &motors[m];

      if (motor->moving)
      {
        int32_t dir = updateMotorVelocity(motor, DATA_RATE_TIME_SEGMENT);
        setMotorDir(m, dir);
      }
    }
  }
  else if (moveState == MOVE_STATE_SHOOT)
  {
    t = goMotionMove.time + DATA_RATE_TIME_SEGMENT;
    goMotionMove.runningTime += DATA_RATE_TIME_SEGMENT;

    if (goMotionMove.mode == GO_MO_MODE_SHOOTING2)
    {
      if (goMotionMove.runningTime >= goMotionMove.shutterCloseTime)
      {
        if (sharedData->cameraValue)
          setCamera(CAMERA_OFF);
      }
      else if (goMotionMove.runningTime >= goMotionMove.shutterOpenTime && !(sharedData->cameraValue & CAMERA_SHUTTER))
      {
        setCamera(CAMERA_SHUTTER | CAMERA_METER);
      }
    }

    if (goMotionMove.state == GO_MO_PRE_ACCEL)
    {
      if (t >= goMotionMove.preAccelDuration)
      {
        goMotionMove.state = GO_MO_ACCEL;
        t -= goMotionMove.preAccelDuration;
      }
    }

    if (goMotionMove.state == GO_MO_ACCEL)
    {
      if (t < goMotionMove.accelDuration)
      {
        tSqr = t * t;
        for (m = 0; m < MOTOR_COUNT; ++m)
        {
          motor = &motors[m];
          if (motor->moving && !(motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL) &&
              (goMotionMove.mode == GO_MO_MODE_RUN_LIVE || (motor->config & DMC_MOTOR_CONFIG_BLUR)))
          {
            xn = (goMotionMove.accelPosition[m] +
                  goMotionMove.acceleration[m] * tSqr); // accel was already multiplied * 0.5

            dx = xn - motor->position;

            motor->currentVelocity = (float)dx * DATA_RATE;

            setMotorDir(m, dx);
            motor->position = xn;
          }
        }
      }
      else
      {
        movePositionFrame = -1; // to trigger update on next pos
        goMotionMove.state = GO_MO_MOVE;
        t -= goMotionMove.accelDuration;

        if (goMotionMove.mode == GO_MO_MODE_SHOOTING)
        {
          setCamera(CAMERA_METER | CAMERA_SHUTTER);
        }
      }
    }

    if (goMotionMove.state == GO_MO_MOVE)
    {
      if (t < goMotionMove.moveDuration)
      {
        float moveTime = goMotionMove.moveStartTime + t * goMotionMove.moveTimeSegment;
        if (goMotionMove.mode == GO_MO_MODE_RUN_LIVE)
        {
          frameTimeMotor.position = FRAME_TO_POSITION(moveTime);

          if ((int32_t)moveTime != movePositionFrame)
          {
            setMovePositionFrame((int32_t)moveTime);
            motorsSendPosition = 1;
          }
        }

        int motorCount = MOTOR_COUNT;
        if (goMotionMove.flags & DMC_RT_CAMERA_STILLS)
        {
          motorCount = MOTOR_CAM_COUNT;
          motors[MOTOR_COUNT].moving = 1;
        }
        else
        {
          motors[MOTOR_COUNT].currentVelocity = 0;
        }

        for (m = 0; m < motorCount; ++m)
        {
          motor = &motors[m];

          if (motor->moving && !(motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL) &&
              (goMotionMove.mode == GO_MO_MODE_RUN_LIVE || (motor->config & DMC_MOTOR_CONFIG_BLUR)))
          {
            if (m == MOTOR_COUNT)
            {
                xn = moveTime * 4096;
            }
            else if (goMotionOverride[m].enabled)
            {
              xn = (goMotionMove.moveP0[m] + goMotionMove.moveV[m] * t);
            }
            else
            {
              xn = evaluateMove(&move[m], moveTime);
            }
            dx = xn - motor->position;

            motor->currentVelocity = (float)dx * DATA_RATE;

            setMotorDir(m, dx);
            motor->position = xn;
          }
        }
      }
      else
      {
        float moveTime = goMotionMove.moveStartTime + goMotionMove.moveDuration * goMotionMove.moveTimeSegment;
        frameTimeMotor.position = FRAME_TO_POSITION(moveTime);
        goMotionMove.state = GO_MO_DECEL;
        
        motors[MOTOR_COUNT].currentVelocity = 0; // stop camera

        t -= goMotionMove.moveDuration;
        if (goMotionMove.mode == GO_MO_MODE_SHOOTING)
        {
          setCamera(CAMERA_OFF);
        }
        else if (goMotionMove.mode == GO_MO_MODE_RUN_LIVE)
        {
          playBlipActive = DATA_RATE / 10;
          sendMotorPositions();

          messageQueue |= DMC_MSG_FLAG_RT_END;
        }
      }
    }

    if (goMotionMove.state == GO_MO_DECEL)
    {
      if (t < goMotionMove.decelDuration)
      {
        tSqr = t * t;
        for (m = 0; m < MOTOR_COUNT; ++m)
        {
          motor = &motors[m];
          if (motor->moving && !(motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL) &&
              (goMotionMove.mode == GO_MO_MODE_RUN_LIVE || (motor->config & DMC_MOTOR_CONFIG_BLUR)))
          {
            xn = (goMotionMove.decelPosition[m] + goMotionMove.decelVelocity[m] * t +
                  goMotionMove.deceleration[m] * tSqr); // accel was already multiplied * 0.5

            dx = xn - motor->position;

            motor->currentVelocity = dx * DATA_RATE;

            setMotorDir(m, dx);
            motor->position = xn;
          }
        }
      }
      else
      {
        goMotionMove.state = GO_MO_POST_DECEL;
        t -= goMotionMove.decelDuration;
      }
    }

    if (goMotionMove.state == GO_MO_POST_DECEL)
    {
      if (t >= goMotionMove.postDecelDuration)
      {
        moveState = MOVE_STATE_JOG;
        goMotionMove.state = GO_MO_INACTIVE;

        for (m = 0; m < MOTOR_COUNT; ++m)
        {
          Motor *motor = &motors[m];
          if (!(motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
          {
            motor->moving = 0;
          }
        }
        motorsSendPosition = 1;

        if (goMotionMove.mode == GO_MO_MODE_RUN_LIVE)
        {
          uint8_t motorsMoving = 0;
          if (goMotionMove.flags & DMC_RT_PLAYBACK_PING_PONG)
          {
            calculateRunLivePingPong(&goMotionMove);
            moveState = MOVE_STATE_SHOOT_WAIT;
            t = 0;
            go(&motorsMoving);
          }
          else if (goMotionMove.flags & DMC_RT_PLAYBACK_LOOP)
          {
            calculateRunLiveLoop(&goMotionMove);
            moveState = MOVE_STATE_SHOOT_WAIT;
            t = 0;
            go(&motorsMoving);
          }
          else
          {
            positionFrame(lround(goMotionMove.moveEndTime));
          }
        }
        playBlipLocation = 0;

        if (goMotionMove.flags & DMC_RT_CAMERA_VIDEO)
        {
          setCamera(CAMERA_METER | CAMERA_SHUTTER);
          cameraTriggerCountdown = 5;
        }
        else
        {
          setCamera(CAMERA_OFF);
        }
      }
    }

    for (m = 0; m < MOTOR_COUNT; ++m)
    {
      motor = &motors[m];
      if ((motor->config & DMC_MOTOR_CONFIG_ENABLED) && (motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL) &&
          motor->moving)
      {
        int32_t dir = updateMotorVelocity(motor, DATA_RATE_TIME_SEGMENT);
        setMotorDir(m, dir);
      }
    }

    goMotionMove.time = t;
  }
  else if (moveState == MOVE_STATE_ALL_JOG)
  {
    int32_t dir = updateMotorVelocity(&frameTimeMotor, DATA_RATE_TIME_SEGMENT); // update time
    if (dir)
    {
      frameTimeMotorDir = (dir == 1) ? 1 : 0;
    }

    // run any 'live control' motors
    for (m = 0; m < MOTOR_COUNT; ++m)
    {
      motor = &motors[m];
      const uint8_t liveControlEnabled = (DMC_MOTOR_CONFIG_ENABLED | DMC_MOTOR_CONFIG_LIVE_CONTROL);
      if ((motor->config & liveControlEnabled) == liveControlEnabled && motor->moving)
      {
        int32_t dir = updateMotorVelocity(motor, DATA_RATE_TIME_SEGMENT);
        setMotorDir(m, dir);
      }
    }

    if (frameTimeMotor.moving)
    {
      t = POSITION_TO_FRAME(frameTimeMotor.position);

      if ((int32_t)t != movePositionFrame)
      {
        setMovePositionFrame((int32_t)t);
        motorsSendPosition = 1;
      }

      t = 1 + (t - moveStartFrame);

      for (m = 0; m < MOTOR_COUNT; ++m)
      {
        motor = &motors[m];
        if ((motor->config & DMC_MOTOR_CONFIG_ENABLED) && move[m].frameCount)
        {
          if (!(motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
          {
            xn = evaluateMove(&move[m], t);

            dx = xn - motor->position;

            motor->moving = 1;
            motor->currentVelocity = dx * DATA_RATE;

            setMotorDir(m, dx);

            motor->position = xn;
          }
        }
      }
    }
    else
    {
      // if frameTime is not moving, must stop motors
      for (m = 0; m < MOTOR_COUNT; ++m)
      {
        motor = &motors[m];
        if ((motor->config & DMC_MOTOR_CONFIG_ENABLED) && !(motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL))
        {
          motor->moving = 0;
        }
      }

      // if we no longer need to be in jog_all, change state to normal jog
      if (goMotionMove.state == GO_MO_INACTIVE)
      {
        moveState = MOVE_STATE_JOG;
      }
    }
  }
  else if (moveState == MOVE_STATE_SHOOT_WAIT)
  {
    for (m = 0; m < MOTOR_COUNT; ++m)
    {
      motor = &motors[m];
      if ((motor->config & DMC_MOTOR_CONFIG_ENABLED) && (motor->config & DMC_MOTOR_CONFIG_LIVE_CONTROL) &&
          motor->moving)
      {
        int32_t dir = updateMotorVelocity(motor, DATA_RATE_TIME_SEGMENT);
        setMotorDir(m, dir);
      }
    }
  }

  for (m = 0; m < MOTOR_CAM_COUNT; ++m)
  {
    motor = &motors[m];
    int64_t speed = (int64_t)(roundf(motor->currentVelocity * 21474.83648f));
    sharedData->nextSpeed[m] = speed;

    if (motor->config & (DMC_MOTOR_CONFIG_COUPLE | DMC_MOTOR_CONFIG_COUPLE_R))
    {
      int m2 = m + 1;
      if (m2 < MOTOR_COUNT)
      {
        int32_t dir = getMotorDir(m);
        dir = (dir) ? 1 : -1;
        if (motor->config & DMC_MOTOR_CONFIG_COUPLE_R)
        {
          speed = -speed;
          dir = -dir;
        }
        setMotorDir(m2, dir);
        sharedData->nextSpeed[m2] = speed;
        ++m;
      }
    }
  }

  sharedData->motorDirection = motorDirection;
  sharedData->motorDataLoaded = 1;

  if (shouldHardStop)
  {
    stopAll((shouldHardStop == 2) ? 1 : 0);
  }

  return 1;
}

void setMotorDir(int32_t m, float d)
{
  if (d > 0.0001f)
  {
    motorDirection |= (1U << m);
  }
  else if (d < -0.0001f)
  {
    motorDirection &= ~(1U << m);
  }
}

int32_t getMotorDir(int32_t m)
{
  return ((motorDirection & (1U << m)) != 0);
}

int32_t updateMotorVelocity(Motor *motor, float timeSegment)
{
  int32_t dir = 0;

  if (motor->moving)
  {
    if (motor->stopping == 2)
    {
      motor->currentVelocity *= 0.975f;

      float dx = motor->currentVelocity * DATA_RATE_TIME_SEGMENT;

      if (dx > 0.0001f)
      {
        dir = 1;
      }
      else if (dx < -0.0001f)
      {
        dir = -1;
      }
      else
      {
        dx = 0;
        motor->stopping = 0;
        motor->moving = 0;
        motor->currentVelocity = 0;
        motorsSendPosition = 1;
      }
      motor->position += dx;
    }
    else
    {
      MotorMove *mv = &motor->moves[motor->currentMove];

      if (mv->time == 0.0f) // this is same because I would have actually set it to zero
      {
        motor->moving = 0;
        motor->currentVelocity = 0;

        motorsSendPosition = 1;
      }
      else
      {
        motor->currentMoveTime += timeSegment;
        if (motor->currentMoveTime >= mv->time)
        {
          motor->currentMoveTime -= mv->time;
          ++motor->currentMove;
          mv = &motor->moves[motor->currentMove];
        }
        float t = motor->currentMoveTime;
        float xn = (mv->position + mv->velocity * t + mv->acceleration * t * t); // accel was already multiplied * 0.5

        float dx = xn - motor->position;

        motor->currentVelocity = dx * DATA_RATE;

        if (dx > 0.0001f)
        {
          dir = 1;
        }
        else if (dx < -0.0001f)
        {
          dir = -1;
        }

        motor->position = xn;
      }
    }
  }
  return dir;
}

void stopAll(uint8_t emergency)
{
  int32_t m;

  unsigned long msTime = millis();
  if (stopAllLastTime < msTime && (msTime - stopAllLastTime) < 1000)
  {
    emergency = 1;
  }

  motors[MOTOR_COUNT].currentVelocity = 0; // stop camera
  if (goMotionMove.flags & DMC_RT_CAMERA_VIDEO)
  {
    setCamera(CAMERA_METER | CAMERA_SHUTTER);
    cameraTriggerCountdown = 5;
  }

  stopAllLastTime = msTime;

  if (moveState == MOVE_STATE_ALL_JOG)
  {
    frameTimeMotor.maxAcceleration = FRAME_TO_POSITION(4);
    float maxAcceleration = frameTimeMotor.maxAcceleration;
    if (frameTimeStopCounter == 0)
    {
      frameTimeMotor.maxAcceleration =
        fmaxf(FRAME_TO_POSITION(2), fmaxf(frameTimeMotor.maxVelocity, fabsf(frameTimeMotor.currentVelocity)) * 0.5f);
    }
    else if (frameTimeStopCounter == 1)
    {
      frameTimeMotor.maxAcceleration =
        fmaxf(FRAME_TO_POSITION(1), fmaxf(frameTimeMotor.maxVelocity, fabsf(frameTimeMotor.currentVelocity)) * 2.0f);
    }
    else
    {
      return;
    }
    ++frameTimeStopCounter;

    stopMotor(&frameTimeMotor, -2, 0);
    frameTimeMotor.maxAcceleration = maxAcceleration;
  }
  else
  {
    moveState = MOVE_STATE_JOG;

    for (m = 0; m < MOTOR_COUNT; ++m)
    {
      float maxAcceleration = motors[m].maxAcceleration;

      motors[m].maxAcceleration =
        fmaxf(motors[m].maxAcceleration, fabsf(motors[m].currentVelocity)); // let's hard stop here, folks

      if (goMotionMove.state && goMotionMove.mode != GO_MO_MODE_RUN_LIVE)
      {
        motors[m].maxAcceleration *= 0.5f;
      }

      stopMotor(&motors[m], m, emergency);

      motors[m].maxAcceleration = maxAcceleration;
    }
    goMotionMove.state = GO_MO_INACTIVE;
  }
}

int32_t stopMotor(Motor *motor, int32_t motorIndex, int8_t emergency)
{
  if (!motor->moving || motor->stopping)
    return 0;

  memset((char *)motor->moves, 0, P2P_MOVE_COUNT * sizeof(MotorMove));

  float v = motor->currentVelocity;

  float maxA = motor->maxAcceleration;
  if (motorIndex >= MOTOR_COUNT || motorIndex == -1)
  {
    maxA *= 1.5f;
  }

  float t = fabsf(v / maxA);

  // for frame time, try to land on frame
  if (motor == &frameTimeMotor)
  {
    float accel = (v > 0) ? -maxA : maxA;
    int32_t pos = (int32_t)(motor->position + v * t + 0.5f * accel * t * t);

    int32_t startPos = FRAME_TO_POSITION(moveStartFrame);
    int32_t endPos = FRAME_TO_POSITION(moveStartFrame + moveFrameCount);
    int32_t changed = 0;

    if (pos < startPos)
    {
      pos = startPos;
      changed = 1;
    }
    else if (pos > endPos)
    {
      pos = endPos;
      changed = 1;
    }
    else if (pos % 100000L)
    {
      pos = 100000L * ((pos / 100000L) + ((v > 0) ? 1 : 0));
      changed = 2;
    }

    if (changed)
    {
      t = 2.0f * fabsf((pos - motor->position) / v);
      maxA = fabsf(v / t);

      if (t > 2.0f && changed == 2 && fabsf(motor->position - pos) < 100000)
      {
        float mv = frameTimeMotor.maxVelocity;
        frameTimeMotor.maxVelocity = FRAME_TO_POSITION(1);
        calculatePointToPoint(&frameTimeMotor, pos, 0.0f);
        frameTimeMotor.maxVelocity = mv;
        return 0;
      }
    }
  }

  // consider limits!
  if (v > 0 && motor->limitHighEnabled)
  {
    if (motor->limitHigh > motor->position)
    {
      float a = (v * v) / (2.0f * (motor->limitHigh - motor->position));
      if (a > maxA)
      {
        maxA = a;
        t = fabsf(v / maxA);
      }
    }
  }
  else if (v < 0 && motor->limitLowEnabled)
  {
    if (motor->limitLow < motor->position)
    {
      float a = (v * v) / (2.0f * (motor->position - motor->limitLow));
      if (a > maxA)
      {
        maxA = a;
        t = fabsf(v / maxA);
      }
    }
  }

  motor->stopping = 1;
  motor->moves[0].time = t;
  motor->moves[0].position = motor->position;
  motor->moves[0].velocity = v;
  motor->moves[0].acceleration = (v > 0) ? -maxA : maxA;

  motor->moves[1].time = 0;
  motor->moves[1].position =
    (motor->moves[0].position + motor->moves[0].velocity * t + 0.5f * motor->moves[0].acceleration * t * t);
  int32_t intPos = (int32_t)motor->moves[1].position;
  if (fabsf(motor->moves[1].position - intPos) > 0.01f)
  {
    if (v > 0)
      motor->moves[1].position = intPos + 1;
    else
      motor->moves[1].position = intPos - 1;
  }
  motor->moves[1].velocity = 0;
  motor->moves[1].acceleration = 0;

  motor->moves[0].acceleration *= 0.5f;

  motor->currentMoveTime = 0;
  motor->currentMove = 0;

  movePositionFrame = -1;

  return 1;
}
