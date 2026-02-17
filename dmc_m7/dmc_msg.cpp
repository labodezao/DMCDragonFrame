/*
 * dmc_msg.c
 *
 * DMC message parser
 * Modified: 2021-02-16
 *
 * Created by DZED Systems LLC
 *
 * We grant you permission to use this source file directly or to modify as needed.
 *
 */

#include "dmc_msg.h"

#include <stdlib.h>
#include <string.h>

#define MSG_STATE_START 0
#define MSG_STATE_HEADER 1
#define MSG_STATE_DATA 2
#define MSG_STATE_AX_DATA 3

#define MSG_CSUM_LENGTH 2

#define DMC_HEADER_SIZE 10
#define DMC_HEADER_ID_OFFSET 2
#define DMC_HEADER_TYPE_OFFSET 6
#define DMC_HEADER_LENGTH_OFFSET 8

#define DMC_INPUT_BUFFER_SIZE 2048
#define MAX_DATA_LENGTH (DMC_INPUT_BUFFER_SIZE - DMC_HEADER_SIZE)

#define DMC_MSG_ID(msg)                                                                                                \
  (msg[DMC_HEADER_ID_OFFSET] | (msg[DMC_HEADER_ID_OFFSET + 1] << 8) | (msg[DMC_HEADER_ID_OFFSET + 2] << 16) |          \
   (msg[DMC_HEADER_ID_OFFSET + 3] << 24))
#define DMC_MSG_TYPE(msg) (msg[DMC_HEADER_TYPE_OFFSET] | (msg[DMC_HEADER_TYPE_OFFSET + 1] << 8))
#define DMC_MSG_LENGTH(msg) (msg[DMC_HEADER_LENGTH_OFFSET] | (msg[DMC_HEADER_LENGTH_OFFSET + 1] << 8))

static uint8_t dmc_last_user_data;
static int32_t dmc_msg_state;
static uint8_t *dmc_user_msg = 0;
static uint16_t dmc_user_msg_index;
static uint16_t dmc_user_msg_read_index;

static uint8_t *dmc_out_msg = 0;
static uint16_t dmc_out_msg_index;

uint16_t dmc_msg_compute_csum(uint8_t *data, uint16_t bytes);

void dmc_msg_init()
{
  dmc_msg_state = MSG_STATE_START;
  dmc_last_user_data = 0;
  dmc_user_msg_index = 0;

  dmc_user_msg = (uint8_t *)malloc(DMC_INPUT_BUFFER_SIZE);
  dmc_user_msg[0] = 'D';
  dmc_user_msg[1] = 'F';

  dmc_out_msg_index = 0;

  dmc_out_msg = (uint8_t *)malloc(256);
  dmc_out_msg[0] = 'D';
  dmc_out_msg[1] = 'F';
}

void dmc_msg_destroy()
{
  free(dmc_user_msg);
  free(dmc_out_msg);
}

/*
 * uint16_t dmc_msg_process_character(uint8_t data, uint8_t * validChecksum, uint32_t * msgId, uint16_t * msgLength)
 *
 * Process single byte of user data
 *
 * Returns command code for completed command.
 */
uint16_t dmc_msg_process_character(uint8_t data, uint8_t *validChecksum, uint32_t *msgId, uint16_t *msgLength)
{
  uint16_t cmd = 0;

  switch (dmc_msg_state)
  {
  case MSG_STATE_START:
    if (data == 'F' && dmc_last_user_data == 'D')
    {
      dmc_user_msg_index = 2;
      dmc_last_user_data = 0;
      dmc_msg_state = MSG_STATE_HEADER;
    }
    else
    {
      dmc_last_user_data = data;
    }
    break;
  case MSG_STATE_HEADER:
    dmc_user_msg[dmc_user_msg_index++] = data;
    if (dmc_user_msg_index == DMC_HEADER_SIZE)
    {
      if (DMC_MSG_LENGTH(dmc_user_msg) + MSG_CSUM_LENGTH > MAX_DATA_LENGTH)
      {
        dmc_msg_state = MSG_STATE_START;
      }
      else if (DMC_MSG_TYPE(dmc_user_msg) & ~0x83FF)
      {
        // invalid message type
        dmc_msg_state = MSG_STATE_START;
      }
      else
      {
        dmc_msg_state = MSG_STATE_DATA;
      }
    }
    break;
  case MSG_STATE_DATA:
    dmc_user_msg[dmc_user_msg_index++] = data;
    if (dmc_user_msg_index == DMC_MSG_LENGTH(dmc_user_msg) + MSG_CSUM_LENGTH + DMC_HEADER_SIZE)
    {
      dmc_msg_state = MSG_STATE_START;
      cmd = DMC_MSG_TYPE(dmc_user_msg);

      *msgId = DMC_MSG_ID(dmc_user_msg);
      *msgLength = DMC_MSG_LENGTH(dmc_user_msg);

      if (dmc_msg_compute_csum(dmc_user_msg, dmc_user_msg_index))
      {
        *validChecksum = 0;
      }
      else
      {
        *validChecksum = 1;

        dmc_user_msg_index -= 2; // remove checksum bytes
        dmc_user_msg_read_index = DMC_HEADER_SIZE;
      }
    }

    break;
  }

  return cmd;
}

uint8_t dmc_msg_read_byte()
{
  uint8_t value = 0;
  if (dmc_user_msg_read_index < dmc_user_msg_index)
  {
    value = dmc_user_msg[dmc_user_msg_read_index];
    ++dmc_user_msg_read_index;
  }
  return value;
}

uint16_t dmc_msg_read_word()
{
  uint16_t value = 0;
  if (dmc_user_msg_read_index + 2 <= dmc_user_msg_index)
  {
    memcpy(&value, dmc_user_msg + dmc_user_msg_read_index, 2);
    dmc_user_msg_read_index += 2;
  }
  else
  {
    dmc_user_msg_read_index = dmc_user_msg_index;
  }
  return value;
}

uint32_t dmc_msg_read_dword()
{
  uint32_t value = 0;
  if (dmc_user_msg_read_index + 4 <= dmc_user_msg_index)
  {
    memcpy(&value, dmc_user_msg + dmc_user_msg_read_index, 4);
    dmc_user_msg_read_index += 4;
  }
  else
  {
    dmc_user_msg_read_index = dmc_user_msg_index;
  }
  return value;
}

uint8_t dmc_msg_read_at_end()
{
  return dmc_user_msg_read_index >= dmc_user_msg_index;
}

int32_t dmc_msg_read_left()
{
  return dmc_user_msg_index - dmc_user_msg_read_index;
}

uint16_t dmc_msg_compute_csum(uint8_t *data, uint16_t bytes)
{
  uint16_t sum1 = 0, sum2 = 0;
  uint16_t tlen;

  while (bytes)
  {
    tlen = ((bytes >= 20) ? 20 : bytes);
    bytes -= tlen;
    do
    {
      sum2 += sum1 += *data++;
      --tlen;
    } while (tlen);
    sum1 %= 0xff;
    sum2 %= 0xff;
  }
  return (sum2 << 8) | sum1;
}

void dmc_msg_prepare(uint16_t type, uint32_t id)
{
  dmc_out_msg_index = 2;
  dmc_msg_out_dword(id);
  dmc_msg_out_word(type);
  dmc_msg_out_word(0); // length
#ifdef DMC_SIMULATOR
  qDebug("MSG OUT TYPE=0x%X, ID=%d", type, id);
#endif
}

void dmc_msg_out_byte(uint8_t data)
{
  dmc_out_msg[dmc_out_msg_index++] = data;
}

void dmc_msg_out_word(uint16_t data)
{
  memcpy(((char *)dmc_out_msg) + dmc_out_msg_index, &data, 2);
  dmc_out_msg_index += 2;
}

void dmc_msg_out_dword(uint32_t data)
{
  memcpy(((char *)dmc_out_msg) + dmc_out_msg_index, &data, 4);
  dmc_out_msg_index += 4;
}

uint8_t *dmc_msg_finalize_output(uint16_t *msgLength)
{
  uint16_t length = dmc_out_msg_index - DMC_HEADER_SIZE;
  memcpy(((char *)dmc_out_msg) + DMC_HEADER_LENGTH_OFFSET, &length, 2);

  // compute checksum
  uint16_t csum = dmc_msg_compute_csum(dmc_out_msg, dmc_out_msg_index);
  uint8_t c0, c1, f0, f1;
  f0 = csum & 0xff;
  f1 = (csum >> 8) & 0xff;
  c0 = 0xff - ((f0 + f1) % 0xff);
  c1 = 0xff - ((f0 + c0) % 0xff);
  dmc_msg_out_byte(c0);
  dmc_msg_out_byte(c1);

  *msgLength = dmc_out_msg_index;

  return (uint8_t *)dmc_out_msg;
}
