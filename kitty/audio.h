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
  bool has_rate;
  bool has_channels;
  bool has_more;
  bool has_data_sz;
  bool has_data_offset;
  bool has_timestamp;
  bool has_preroll;
  bool has_autoplay;
  bool has_loop_count;
  bool has_volume;
  bool has_seek;
  bool has_playback_state;
  bool has_id;
  bool has_quiet;
  size_t payload_sz;
} AudioCommand;



#include "audio_stream.h"
#include "audio_transport.h"
