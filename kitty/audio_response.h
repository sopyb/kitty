#pragma once

#include "data-types.h"

typedef struct AudioStream AudioStream;

typedef enum {
  AUDIO_RESPONSE_OK = 0,
  AUDIO_RESPONSE_EINVAL = 1,
  AUDIO_RESPONSE_ENOENT = 2,
  AUDIO_RESPONSE_EUNSUPPORTED = 3,
  AUDIO_RESPONSE_ENOSPC = 4,
  AUDIO_RESPONSE_EIO = 5,
} AudioResponseCode;

void audio_send_response(void *self, uint8_t action, uint32_t id,
                         AudioResponseCode code, const char *message,
                         uint8_t quiet);
void audio_send_capability_response(void *self, uint32_t id, uint8_t quiet);
void audio_send_stream_state_response(void *self, const AudioStream *stream,
                                      uint32_t id, uint8_t quiet);
