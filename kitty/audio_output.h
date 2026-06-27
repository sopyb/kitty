#pragma once

#include <stdbool.h>

typedef struct AudioStream AudioStream;

bool audio_output_start(AudioStream *stream);
void audio_output_pause(AudioStream *stream);
void audio_output_resume(AudioStream *stream);
void audio_output_stop(AudioStream *stream);
