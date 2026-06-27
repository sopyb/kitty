#include "audio_backend.h"
#include <stddef.h>

const AudioBackend *audio_backend_select(void) {
#if defined(__APPLE__)
  (void)0; /* CoreAudio support removed */
  return NULL;
#else
  const AudioBackend *alsa = audio_backend_alsa();
  if (alsa && alsa->available && alsa->available()) {
    return alsa;
  }
  return NULL;
#endif
}
