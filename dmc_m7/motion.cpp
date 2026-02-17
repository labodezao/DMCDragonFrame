/*
 * motion.c
 * dmc-lite source code
 * Copyright 2023 by DZED Systems LLC
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dfx.h"
#include "dmc_msg.h"
#include "motion.h"

void calculatePointToPoint(Motor *motor, int32_t destination, float minTime)
{
  int32_t i, moveCount;
  int32_t moveProgrammed = 0;

  moveCount = 0;

  float motorPosition = motor->position;
  if (fabsf(motorPosition - destination) < 1.0f && !motor->moving)
  {
    return;
  }

  float maxA = motor->maxAcceleration;
  float maxV = motor->maxVelocity;
  if (maxV == 0.0f)
  {
    maxV = 100.0f;
  }
  if (maxA == 0.0f)
  {
    maxA = maxV;
  }
  float maxAInverse = 1.0f / maxA;

  memset(motor->moves, 0, P2P_MOVE_COUNT * sizeof(MotorMove));

  if (motor->maxAcceleration == 0)
  {
    i = 0;
    while (1)
    {
      i++;
      // IOWR_32DIRECT(LED_BASE, 0, (i >> 8) );//leds);
    }
  }

  float tmax = maxV * maxAInverse;
  float dmax = maxV * tmax;

  float dist = fabsf((float)(destination - motorPosition));
  int8_t dir = destination > motorPosition ? 1 : -1;

  motor->moves[0].position = motorPosition;
  motor->currentMoveTime = 0;

  if (fabsf(motor->currentVelocity) > 0.001f) // we need to account for existing velocity
  {
    float vi = motor->currentVelocity;
    float ti = fabsf(vi * maxAInverse);
    float di = 0.5f * maxA * ti * ti;

    if (vi * dir < 0) // switching directions
    {
      motor->moves[moveCount].time = ti;
      motor->moves[moveCount].acceleration = dir * maxA;
      motor->moves[moveCount].velocity = vi;
      moveCount++;

      dist += di;
    }
    else if (dist < di) // must decelerate and switch directions
    {
      motor->moves[moveCount].time = ti;
      motor->moves[moveCount].acceleration = -dir * maxA;
      motor->moves[moveCount].velocity = vi;
      moveCount++;

      dist = (di - dist);
      dir = -dir;
    }
    else // further in same direction
    {
      float deltaV = fabsf(vi) - maxV;

      if (deltaV > 0)
      {
        float tDecel = fabsf(deltaV * maxAInverse);

        motor->moves[moveCount].time = tDecel;
        motor->moves[moveCount].acceleration = -dir * maxA;
        motor->moves[moveCount].velocity = vi;

        float dDecel = fabsf(vi * tDecel + 0.5f * -dir * maxA * tDecel * tDecel);

        dist -= dDecel;

        dist -= 0.5f * dmax;

        float tconst = dist / maxV;

        moveCount++;
        motor->moves[moveCount].time = tconst;
        motor->moves[moveCount].acceleration = 0;

        moveCount++;
        motor->moves[moveCount].time = tmax;
        motor->moves[moveCount].acceleration = dir * -maxA;

        moveProgrammed = 1;
      }
      else
      {
        // this does not work if velocity, accel are changed
        dist += di;
        motor->moves[moveCount].position -= (dir * di);
        motor->currentMoveTime = ti;
      }
    }
  }

  if (!moveProgrammed)
  {
    float t = sqrtf(dist * maxAInverse);

    // minTime?
    if (fabsf(motor->currentVelocity) < 0.001f && minTime > 0)
    {
      /*
       * First make sure move takes at least required time
       */
      if (t < minTime)
      {
        maxA = dist / (minTime * minTime);
        tmax = maxV / maxA;
        dmax = maxV * tmax;
      }

      /*
       * Get rid of constant velocity phase ( /----\ ) by changing accel ( /\ )
       * not sure if this is best approach. only for shortish distances
       */
      if (dist > dmax && dist < dmax * 2)
      {
        maxA = (maxV * maxV) / dist;
        tmax = maxV / maxA;
        dmax = dist;
      }
      t = sqrtf(dist / maxA);
    }

    if (dist <= dmax)
    {
      motor->moves[moveCount].time = t;
      motor->moves[moveCount].acceleration = dir * maxA;

      moveCount++;
      motor->moves[moveCount].time = t;
      motor->moves[moveCount].acceleration = dir * -maxA;
    }
    else
    {
      dist -= dmax;

      float tconst = dist / maxV;

      motor->moves[moveCount].time = tmax;
      motor->moves[moveCount].acceleration = dir * maxA;

      moveCount++;
      motor->moves[moveCount].time = tconst;
      motor->moves[moveCount].acceleration = 0;

      moveCount++;
      motor->moves[moveCount].time = tmax;
      motor->moves[moveCount].acceleration = dir * -maxA;
    }
  }

  for (i = 1; i <= moveCount; ++i)
  {
    MotorMove *c = &motor->moves[i];
    MotorMove *last = &motor->moves[i - 1];

    float t = last->time;

    c->position = (last->position + last->velocity * t + 0.5f * last->acceleration * t * t);
    if (fabsf(c->position - destination) < 0.001f)
    {
      c->acceleration = 0;
    }
    else
    {
      c->velocity = last->velocity + last->acceleration * t;
    }
  }

  motor->moves[moveCount + 1].position = destination;

  for (i = 0; i <= moveCount; ++i)
  {
    MotorMove *c = &motor->moves[i];
    c->acceleration *= 0.5f; // pre-multiply here for later position calculation
  }

  motor->stopping = 0;
  motor->currentMove = 0;
  if (!motor->moving)
  {
    motor->moving = 1;
  }
}

/*
 * t is "frame" time, offset to start frame
 */
float evaluateMove(AxisMoveData *axisMove, float t)
{
  if (t < 0)
  {
    float dx = axisMove->position[1] - axisMove->position[0];
    return axisMove->position[0] + dx * t;
  }

  int32_t pos = (int32_t)(t);
  if (pos >= axisMove->frameCount)
  {
    return axisMove->position[axisMove->frameCount];
  }

  float u = t - pos;
  float dx = axisMove->position[pos + 1] - axisMove->position[pos];
  return axisMove->position[pos] + dx * u;
}

void calculateGoMotionMove(Motor *motors, AxisMoveData *axisMoves, GoMotionMove *goMotionMove, int32_t frame,
                           int32_t dir, int32_t exposure, int32_t blur, GoMotionOverride *overrides)
{
  float accelTime = 1.0f;
  float decelTime = 1.0f;

  float dt = dir * blur * 0.0005f; // blur is 0-1000, but this is half of blur

  float timeStart = frame - dt;  // start time
  float timeFinish = frame + dt; // finish time

  float speedFactor = ((1000.0f / exposure) * (blur * 0.001f));

  int32_t m;

  goMotionMove->time = 0;
  goMotionMove->state = GO_MO_PREROLL;
  goMotionMove->accelDuration = 0;
  goMotionMove->decelDuration = 0;

  goMotionMove->moveDuration = fabsf(timeFinish - timeStart) / speedFactor;
  goMotionMove->moveStartTime = timeStart;
  goMotionMove->moveTimeSegment = speedFactor * dir;

  // set up acceleration and deceleration
  for (m = 0; m < MOTOR_COUNT; m++)
  {
    Motor *motor = &motors[m];
    AxisMoveData *axisMove = &axisMoves[m];

    if (!(motor->config & DMC_MOTOR_CONFIG_ENABLED) || !axisMove->frameCount)
    {
      goMotionMove->acceleration[m] = 0;
      goMotionMove->deceleration[m] = 0;
      continue;
    }

    if (!(motor->config & DMC_MOTOR_CONFIG_BLUR) || blur == 0)
    {
      float pm = evaluateMove(axisMove, frame);
      goMotionMove->accelPosition[m] = (int32_t)round(pm);
      goMotionMove->decelPosition[m] = (int32_t)round(pm);
      continue;
    }

    float p1 = evaluateMove(axisMove, timeStart);
    float f1 = evaluateMove(axisMove, timeFinish);

    float v;

    if (overrides[m].enabled)
    {
      p1 = overrides[m].posA;
      f1 = overrides[m].posB;

      float delta = (0.5f - fabsf(dt)) * (f1 - p1);
      f1 = f1 - delta;
      p1 += delta;

      v = (f1 - p1) / goMotionMove->moveDuration;

      goMotionMove->moveP0[m] = p1;
      goMotionMove->moveP1[m] = f1;
      goMotionMove->moveV[m] = v;
    }
    else
    {
      float p2 = evaluateMove(axisMove, timeStart + dir * 0.1f);
      v = speedFactor * (p2 - p1) * 10.0f;
    }

    // v = a*t -> a = v / t
    float a = v / accelTime;
    float dp = 0.5f * a * accelTime * accelTime;
    float sp = (int32_t)round(p1 - (dp + 0.5f)); // starting position
    dp = p1 - sp;
    a = dp * 2.0f / (accelTime * accelTime);

    goMotionMove->accelPosition[m] = sp;
    goMotionMove->acceleration[m] = a * 0.5f; // pre-multiply for formulas

    // deceleration
    if (!overrides[m].enabled)
    {
      float f2 = evaluateMove(axisMove, timeFinish - dir * 0.1f);
      v = speedFactor * (f1 - f2) * 10.0f;
    }

    // v = a*t -> a = v / t
    a = -v / decelTime;

    dp = 0.5f * a * accelTime * accelTime;
    sp = (int32_t)round(f1 + (dp + 0.5f)); // starting position
    dp = sp - f1;
    a = dp * 2.0f / (accelTime * accelTime);

    goMotionMove->decelVelocity[m] = v;
    goMotionMove->decelPosition[m] = f1;
    goMotionMove->deceleration[m] = a * 0.5f; // pre-multiple for formulas
  }
  goMotionMove->accelDuration = accelTime;
  goMotionMove->decelDuration = decelTime;

  goMotionMove->mode = GO_MO_MODE_SHOOTING;
}

void calculateGoMotionMove2(Motor *motors, AxisMoveData *axisMoves, GoMotionMove *goMotionMove, int32_t frame,
                            int32_t exposure, int16_t shutterOpen, int16_t shutterClosed, GoMotionOverride *overrides)
{

  float exposureTime = exposure * 0.001f;

  // movement from P0 to P1 happens 0-360 degrees
  // but shutterOpen -> shutterClosed = exposureTime, no matter how many degrees it is!

  float shutterDegrees = (shutterClosed - shutterOpen);
  float secondsPerDegree = exposureTime / shutterDegrees;

  if (shutterOpen < 0)
  {
    goMotionMove->shutterOpenTime = 0;
    goMotionMove->preAccelDuration = -shutterOpen * secondsPerDegree;
  }
  else
  {
    goMotionMove->shutterOpenTime = shutterOpen * secondsPerDegree;
  }

  if (shutterClosed > 360)
  {
    goMotionMove->postDecelDuration = (shutterClosed - 360.0f) * secondsPerDegree;
  }
  goMotionMove->shutterCloseTime = goMotionMove->shutterOpenTime + exposureTime;

  float moveTime = 360.0f * secondsPerDegree;
  float accelTime = moveTime * 0.125f;
  float decelTime = moveTime * 0.125f;

  int32_t m;

  goMotionMove->time = 0;
  goMotionMove->state = GO_MO_PREROLL;

  goMotionMove->moveDuration = moveTime * 0.75f;
  goMotionMove->moveStartTime = 0.125f;
  goMotionMove->moveTimeSegment = 1.0f;

  // set up acceleration and deceleration
  for (m = 0; m < MOTOR_COUNT; m++)
  {
    Motor *motor = &motors[m];
    AxisMoveData *axisMove = &axisMoves[m];

    if (!(motor->config & DMC_MOTOR_CONFIG_ENABLED) || !axisMove->frameCount)
    {
      goMotionMove->acceleration[m] = 0;
      goMotionMove->deceleration[m] = 0;
      continue;
    }

    if ((motor->config & DMC_MOTOR_CONFIG_BLUR))
    {
      float p0, p1;
      if (overrides[m].enabled)
      {
        p0 = overrides[m].posA;
        p1 = overrides[m].posB;
      }
      else
      {
        overrides[m].enabled = 1; // we do this to trigger proper calculations
        p0 = (int32_t)round(evaluateMove(axisMove, frame - 0.5f));
        p1 = (int32_t)round(evaluateMove(axisMove, frame + 0.5f));
      }

      float v = (8.0f / 7.0f) * (p1 - p0) / moveTime;

      float a = v / accelTime;

      goMotionMove->accelPosition[m] = p0;
      goMotionMove->acceleration[m] = a * 0.5f; // pre-multiply for formulas

      float dp = 0.5f * a * accelTime * accelTime;

      goMotionMove->moveP0[m] = p0 + dp;
      goMotionMove->moveP1[m] = p1 - dp;
      goMotionMove->moveV[m] = v;

      goMotionMove->decelPosition[m] = goMotionMove->moveP1[m];
      goMotionMove->decelVelocity[m] = v;
      goMotionMove->deceleration[m] = a * -0.5f; // pre-multiply for formulas
    }
    else
    {
      float pm = evaluateMove(axisMove, frame);
      goMotionMove->accelPosition[m] = (int32_t)round(pm);
      goMotionMove->decelPosition[m] = (int32_t)round(pm);

      goMotionMove->moveP0[m] = (int32_t)round(pm);
      goMotionMove->moveP1[m] = (int32_t)round(pm);
      goMotionMove->moveV[m] = 0;
    }
  }
  goMotionMove->accelDuration = accelTime;
  goMotionMove->decelDuration = decelTime;

  goMotionMove->mode = GO_MO_MODE_SHOOTING2;
}

void calculateRunLive(Motor *motors, AxisMoveData *axisMoves, GoMotionMove *goMotionMove, int32_t startFrame,
                      int32_t endFrame, float fps, float accelTime, float decelTime)
{
  float timeStart = startFrame;    // start time
  float timeFinish = endFrame + 1; // finish time

  int32_t dir = (endFrame > startFrame) ? 1 : -1;

  float timeDelta = 1.0f;
  float speedFactor = fps;

  int32_t m;

  goMotionMove->time = 0;
  goMotionMove->state = GO_MO_PREROLL;
  goMotionMove->accelDuration = 0;
  goMotionMove->decelDuration = 0;

  goMotionMove->moveDuration = fabsf(timeFinish - timeStart) / speedFactor;
  goMotionMove->moveStartTime = timeStart;
  goMotionMove->moveEndTime = timeFinish;

  goMotionMove->moveTimeSegment = speedFactor * dir;

  float invTimeDelta = 1.0f / timeDelta;

  // set up acceleration and deceleration
  for (m = 0; m < MOTOR_COUNT; m++)
  {
    Motor *motor = &motors[m];
    AxisMoveData *axisMove = &axisMoves[m];

    if (!(motor->config & DMC_MOTOR_CONFIG_ENABLED) || !axisMove->frameCount)
    {
      goMotionMove->acceleration[m] = 0;
      goMotionMove->deceleration[m] = 0;
      continue;
    }

    float p1 = evaluateMove(axisMove, timeStart);
    float f1 = evaluateMove(axisMove, timeFinish);

    float p2 = evaluateMove(axisMove, timeStart + dir * timeDelta);
    float v = speedFactor * (p2 - p1) * invTimeDelta;

    // v = a*t -> a = v / t
    float a = v / accelTime;
    float dp = 0.5f * a * accelTime * accelTime;
    float sp = p1 - dp; // starting position

    goMotionMove->accelPosition[m] = (int32_t)sp;
    goMotionMove->acceleration[m] = a * 0.5f; // pre-multiple for formulas

    // deceleration
    float f2 = evaluateMove(axisMove, timeFinish - dir * timeDelta);
    v = speedFactor * (f1 - f2) * invTimeDelta;

    // v = a*t -> a = v / t
    a = -v / decelTime;

    goMotionMove->decelVelocity[m] = v;
    goMotionMove->decelPosition[m] = (int32_t)f1;
    goMotionMove->deceleration[m] = a * 0.5f; // pre-multiple for formulas
  }
  goMotionMove->accelDuration = accelTime;
  goMotionMove->decelDuration = decelTime;

  goMotionMove->mode = GO_MO_MODE_RUN_LIVE;
}

void calculateRunLivePingPong(GoMotionMove *goMotionMove)
{
  goMotionMove->time = 0;
  goMotionMove->runningTime = 0;
  goMotionMove->state = GO_MO_PREROLL;
  goMotionMove->mode = GO_MO_MODE_RUN_LIVE;

  float startTime = goMotionMove->moveStartTime;
  goMotionMove->moveStartTime = goMotionMove->moveEndTime;
  goMotionMove->moveEndTime = startTime;

  goMotionMove->moveTimeSegment = -goMotionMove->moveTimeSegment;

  goMotionMove->accelDuration = 0;
  goMotionMove->decelDuration = 0;
}

void calculateRunLiveLoop(GoMotionMove *goMotionMove)
{
  goMotionMove->time = 0;
  goMotionMove->runningTime = 0;
  goMotionMove->state = GO_MO_PREROLL;
  goMotionMove->mode = GO_MO_MODE_RUN_LIVE;

  goMotionMove->accelDuration = 0;
  goMotionMove->decelDuration = 0;
}
