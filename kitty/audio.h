#pragma once

#include <stdint.h>
#include <string.h>

typedef struct AudioCommand {
  uint8_t action;
  uint32_t id;
  char format[64];
  uint32_t rate;
  uint32_t channels;
  uint32_t more;
  uint8_t transmission_type;
  uint32_t data_sz;
  uint32_t data_offset;
  uint32_t timestamp;
  uint32_t preroll;
  uint32_t autoplay;
  uint32_t loop_count;
  uint32_t playback_state;
  uint32_t volume;
  uint32_t seek;
  uint32_t quiet;
  bool has_preroll;
  bool has_autoplay;
  bool has_loop_count;
  bool has_volume;
  bool has_seek;
  bool has_playback_state;
  size_t payload_sz;
} AudioCommand;

// Extract format string from control block buffer (e.g., finds s=raw/s16le)
static inline void audio_extract_format(const uint8_t *buf, size_t bufsz,
                                        char *format_out,
                                        size_t format_out_sz) {
  memset(format_out, 0, format_out_sz);
  if (!buf || bufsz < 3)
    return;

  size_t pos = 1; // skip first byte (A discriminator)
  while (pos < bufsz) {
    // Look for 's='
    if (buf[pos] == 's' && pos + 1 < bufsz && buf[pos + 1] == '=') {
      pos += 2; // skip 's='
      size_t out_pos = 0;
      // Copy format string until we hit ',' or ';'
      while (pos < bufsz && out_pos < format_out_sz - 1) {
        if (buf[pos] == ',' || buf[pos] == ';') {
          break;
        }
        format_out[out_pos++] = buf[pos++];
      }
      format_out[out_pos] = '\0';
      return;
    }
    pos++;
  }
}

#include "audio_stream.h"
#include "audio_transport.h"
