/*
 * dmc_m4.ino
 * dmc-lite source code
 * Copyright 2023 by DZED Systems LLC
 *
 * Target core: M4 Co-processor
 * Flash split: 1.5MB M7 + 0.5MB M4
 */

#include <Arduino.h>
#include <RPC.h>
#include <pinDefinitions.h>
#include "config.h"

#define CAMERA_OFF 0x0
#define CAMERA_SHUTTER 0x1
#define CAMERA_METER 0x2

#ifdef CORE_CM7
#error "Make sure to target the M4 Co-processor with flash split 1.5MB M7 + 0.5MB M4"
#endif

#define MOTOR_COUNT 16
#define MOTOR_CAM_COUNT 9

static GPIO_TypeDef *const port_table[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH, GPIOI, GPIOJ, GPIOK};
static const uint16_t mask_table[] = {1 << 0, 1 << 1, 1 << 2,  1 << 3,  1 << 4,  1 << 5,  1 << 6,  1 << 7,
                                      1 << 8, 1 << 9, 1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15};

static inline void digitalWriteFast(pin_size_t pin, PinStatus val)
{
  PinName pin_name = g_APinDescription[pin].name;
  uint16_t mask = mask_table[pin_name & 0xf];
  GPIO_TypeDef *const port = port_table[pin_name >> 4];
  if (val)
    port->BSRR = mask;
  else
    port->BSRR = (uint32_t)(mask << 16);
}

// This version you takes in a pin name (PinName) like LED_RED
static inline void digitalWriteFast(PinName pin_name, PinStatus val) __attribute__((always_inline, unused));
static inline void digitalWriteFast(PinName pin_name, PinStatus val)
{
  uint16_t mask = mask_table[pin_name & 0xf];
  GPIO_TypeDef *const port = port_table[pin_name >> 4];
  if (val)
    port->BSRR = mask;
  else
    port->BSRR = (uint32_t)(mask << 16);
}

int64_t speed[MOTOR_CAM_COUNT];
uint8_t cameraValue;
uint16_t cameraOpenAngle;
uint16_t cameraCloseAngle;

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

DmcSharedData *sharedDataPtr;

TIM_HandleTypeDef htim1;

static const int stepPins[] = { PIN_STEP1, PIN_STEP2, PIN_STEP3, PIN_STEP4, PIN_STEP5, PIN_STEP6, PIN_STEP7, PIN_STEP8 , PIN_STEP9, PIN_STEP10, PIN_STEP11, PIN_STEP12, PIN_STEP13, PIN_STEP15, PIN_STEP15, PIN_STEP16 };
static const int dirPins[] = { PIN_DIR1, PIN_DIR2, PIN_DIR3, PIN_DIR4, PIN_DIR5, PIN_DIR6, PIN_DIR7, PIN_DIR8 , PIN_DIR9, PIN_DIR10, PIN_DIR11, PIN_DIR12, PIN_DIR13, PIN_DIR14, PIN_DIR15, PIN_DIR16};

void setup()
{
  RPC.begin();

  cameraValue = 0;
  sharedDataPtr = 0;
  sharedDataPtr = (DmcSharedData *)0x3800fd00;

  delay(100);
  while (!sharedDataPtr)
  {
    sharedDataPtr = (DmcSharedData *)RPC.call("sharedDataPointer").as<uintptr_t>();
    if (sharedDataPtr)
      break;
    delay(100);
  }

  pinMode(LEDG, OUTPUT);
  for (int i = 0; i < 3; ++i)
  {
    digitalWrite(LEDG, LOW);
    delay(200);
    digitalWrite(LEDG, HIGH);
    delay(200);
  }

  for (int i = 0; i < MOTOR_COUNT; ++i)
  {
    sharedDataPtr->accum[i] = 0;
    speed[i] = 0;
    int stepPin = stepPins[i];
    int dirPin = dirPins[i];
    pinMode(stepPin, OUTPUT);
    digitalWrite(stepPin, LOW);

    pinMode(dirPin, OUTPUT);
    digitalWrite(dirPin, LOW);
  }
  sharedDataPtr->accum[MOTOR_COUNT] = 0;
  speed[MOTOR_COUNT] = 0;

  // set up camera
  pinMode(PIN_CAM_METER, OUTPUT);
  digitalWrite(PIN_CAM_METER, LOW);
  pinMode(PIN_CAM_SHUTTER, OUTPUT);
  digitalWrite(PIN_CAM_SHUTTER, LOW);

  // 200 kHz
  htim1.Instance = TIM1;
#if defined(ARDUINO_ARCH_MBED_PORTENTA)
  htim1.Init.Prescaler = 49; // H7 seems to default to 400 MHz
#else
  htim1.Init.Prescaler = 59; // Giga R1 runs at 480 MHz
#endif
  htim1.Init.Period = 19;
  __HAL_RCC_TIM1_CLK_ENABLE();
  HAL_NVIC_SetPriority(TIM1_UP_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
  HAL_TIM_Base_Init(&htim1);
  HAL_TIM_Base_Start_IT(&htim1);
}

void loop()
{
  while (1)
  {
    delay(1000);
  }
}

extern "C"
{
  void TIM1_UP_IRQHandler(void)
  {
    HAL_TIM_IRQHandler(&htim1);
  }
}

uint32_t counter = 0;
uint32_t ledCounter = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    if (++counter == 4000)
    {
      ++ledCounter;
      if (ledCounter == 90)
      {
        digitalWrite(LEDG, LOW);
      }
      else if (ledCounter == 100)
      {
        ledCounter = 0;
        digitalWrite(LEDG, HIGH);
      }
      counter = 0;

      cameraOpenAngle = sharedDataPtr->cameraOpenAngle;
      cameraCloseAngle = sharedDataPtr->cameraCloseAngle;

      speed[MOTOR_COUNT] = sharedDataPtr->nextSpeed[MOTOR_COUNT]; // camera
      for (int i = 0; i < MOTOR_COUNT; ++i)
      {
        digitalWriteFast(dirPins[i], ((1 << i) & sharedDataPtr->motorDirection) ? HIGH : LOW);
        int64_t prevSpeed = speed[i];
        speed[i] = sharedDataPtr->nextSpeed[i];

        // ensure that we land with the pulse off
        if (prevSpeed && !speed[i] && (sharedDataPtr->accum[i] & 0xFFFFFFFF))
        {
          int32_t before = ((sharedDataPtr->accum[i] >> 31) ^ (sharedDataPtr->accum[i] >> 30)) & 0x1;
          if (before)
          {
            int64_t accum = sharedDataPtr->accum[i];
            bool negative = (accum < 0);
            if (negative)
            {
              accum = -accum;
              prevSpeed = -prevSpeed;
            }
            uint8_t dirSection = (accum >> 30) & 0x3;
            accum = (accum & 0xFFFFFFFF00000000UL);
            if ((prevSpeed > 0 && dirSection) || (prevSpeed < 0 && dirSection == 3))
            {
              accum += (1LL << 32);
            }
            if (negative)
              accum = -accum;
            sharedDataPtr->accum[i] = accum;
            int32_t after = ((sharedDataPtr->accum[i] >> 31) ^ (sharedDataPtr->accum[i] >> 30)) & 0x1;
            if (before != after)
              digitalWriteFast(stepPins[i], after ? HIGH : LOW);
          }
        }
      }
      sharedDataPtr->motorDataLoaded = 0;
    }

    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
      int32_t before = ((sharedDataPtr->accum[i] >> 31) ^ (sharedDataPtr->accum[i] >> 30)) & 0x1;
      sharedDataPtr->accum[i] += speed[i];
      int32_t after = ((sharedDataPtr->accum[i] >> 31) ^ (sharedDataPtr->accum[i] >> 30)) & 0x1;
      if (before != after)
        digitalWriteFast(stepPins[i], after ? HIGH : LOW);
    }

    uint8_t nextCameraPosition;
    if (speed[MOTOR_COUNT])
    {
      sharedDataPtr->accum[MOTOR_COUNT] += speed[MOTOR_COUNT];
      uint32_t camPos = (sharedDataPtr->accum[MOTOR_COUNT] >> 32) & 0xFFF;
      nextCameraPosition = (camPos >= cameraOpenAngle && camPos <= cameraCloseAngle) ? 0x3 : 0x0;
    }
    else
    {
      nextCameraPosition = sharedDataPtr->cameraValue;
    }

    if (cameraValue != nextCameraPosition)
    {
      cameraValue = nextCameraPosition;
      digitalWrite(PIN_CAM_METER, ((cameraValue)&CAMERA_METER) ? HIGH : LOW);
      digitalWrite(PIN_CAM_SHUTTER, ((cameraValue)&CAMERA_SHUTTER) ? HIGH : LOW);
    }
  }
}
