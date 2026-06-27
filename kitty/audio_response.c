#include "audio_response.h"
#include "audio_stream.h"
#include "control-codes.h"
#include <stdio.h>
#include <string.h>

static const char *audio_response_code_to_string(AudioResponseCode code) {
  switch (code) {
  case AUDIO_RESPONSE_OK:
    return "OK";
  case AUDIO_RESPONSE_EINVAL:
    return "EINVAL";
  case AUDIO_RESPONSE_ENOENT:
    return "ENOENT";
  case AUDIO_RESPONSE_EUNSUPPORTED:
    return "EUNSUPPORTED";
  case AUDIO_RESPONSE_ENOSPC:
    return "ENOSPC";
  case AUDIO_RESPONSE_EIO:
    return "EIO";
  default:
    return "UNKNOWN";
  }
}

void audio_send_response(Screen *self, uint8_t action, uint32_t id,
                         AudioResponseCode code, const char *message,
                         uint8_t quiet) {
  if (!self || !self->callbacks)
    return;

  if (quiet == 2 || (quiet == 1 && code == AUDIO_RESPONSE_OK)) {
    return;
  }

  const char *code_str = audio_response_code_to_string(code);
  char response[512];
  int len = 0;

  if (message && strlen(message) > 0) {
    len = snprintf(response, sizeof(response), "Aa=%c,i=%u;%s:%s", action, id,
                   code_str, message);
  } else {
    len = snprintf(response, sizeof(response), "Aa=%c,i=%u;%s", action, id,
                   code_str);
  }

  if (len > 0 && len < (int)sizeof(response)) {
    write_escape_code_to_child(self, ESC_APC, response);
  }
}

void audio_send_capability_response(Screen *self, uint32_t id, uint8_t quiet) {
  if (!self || !self->callbacks)
    return;
  if (quiet == 1 || quiet == 2)
    return;

  char response[512];
  int len = snprintf(response, sizeof(response),
                     "Aa=q,i=%u;OK;V=1,M=1,r=44100|48000,c=1|2,s=raw/s16le", id);

  if (len > 0 && len < (int)sizeof(response)) {
    write_escape_code_to_child(self, ESC_APC, response);
  }
}

static uint64_t audio_stream_position_ms(const AudioStream *stream) {
  if (!stream || stream->rate == 0 || stream->channels == 0)
    return 0;
  size_t bytes_per_sample = audio_format_bytes_per_sample(stream->format);
  if (bytes_per_sample == 0)
    return 0;
  uint64_t frame_size = (uint64_t)bytes_per_sample * stream->channels;
  uint64_t frames = stream->playback_offset / frame_size;
  return (frames * 1000ULL) / stream->rate;
}

static uint64_t audio_stream_duration_ms(const AudioStream *stream) {
  if (!stream || stream->rate == 0 || stream->channels == 0)
    return 0;
  size_t bytes_per_sample = audio_format_bytes_per_sample(stream->format);
  if (bytes_per_sample == 0)
    return 0;
  uint64_t frame_size = (uint64_t)bytes_per_sample * stream->channels;
  uint64_t frames = stream->data_written / frame_size;
  return (frames * 1000ULL) / stream->rate;
}

void audio_send_stream_state_response(Screen *self, const AudioStream *stream,
                                      uint32_t id, uint8_t quiet) {
  if (!self || !self->callbacks || !stream)
    return;
  if (quiet == 1 || quiet == 2)
    return;

  uint64_t k = audio_stream_position_ms(stream);
  uint64_t K = audio_stream_duration_ms(stream);
  char response[512];
  int len =
      snprintf(response, sizeof(response),
               "Aa=q,i=%u;OK;p=%u,v=%u,k=%llu,K=%llu,m=%u,s=%s,r=%u,c=%u,L=%u",
               id, (unsigned int)stream->state, (unsigned int)stream->volume,
               (unsigned long long)k, (unsigned long long)K,
               stream->transmission_complete ? 0u : 1u,
               stream->format == AUDIO_FORMAT_S16LE   ? "raw/s16le"
               : stream->format == AUDIO_FORMAT_U8    ? "raw/u8"
               : stream->format == AUDIO_FORMAT_MULAW ? "raw/mulaw"
               : stream->format == AUDIO_FORMAT_S24LE ? "raw/s24le"
               : stream->format == AUDIO_FORMAT_S32LE ? "raw/s32le"
               : stream->format == AUDIO_FORMAT_F32LE ? "raw/f32le"
               : stream->format == AUDIO_FORMAT_F64LE ? "raw/f64le"
                                                      : "",
               (unsigned int)stream->rate, (unsigned int)stream->channels,
               (unsigned int)stream->loop_count);

  if (len > 0 && len < (int)sizeof(response)) {
    write_escape_code_to_child(self, ESC_APC, response);
  }
}
