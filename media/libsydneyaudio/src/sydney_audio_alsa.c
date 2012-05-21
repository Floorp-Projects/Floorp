/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <pthread.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "sydney_audio.h"

/* ALSA implementation based heavily on sydney_audio_mac.c */

pthread_mutex_t sa_alsa_mutex = PTHREAD_MUTEX_INITIALIZER;

struct sa_stream {
  snd_pcm_t*        output_unit;
  int64_t           bytes_written;
  int64_t           last_position;

  /* audio format info */
  unsigned int      rate;
  unsigned int      n_channels;

  /* work around bug 573924 */
  int               pulseaudio;
  int               resumed;
};

/*
 * -----------------------------------------------------------------------------
 *  Error Handler to prevent output to stderr
 *  ----------------------------------------------------------------------------
 */
static void
quiet_error_handler(const char* file,
                    int         line,
                    const char* function,
                    int         err,
                    const char* format,
                    ...)
{
}

/*
 * -----------------------------------------------------------------------------
 * Startup and shutdown functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_create_pcm(
  sa_stream_t      ** _s,
  const char        * client_name,
  sa_mode_t           mode,
  sa_pcm_format_t     format,
  unsigned  int       rate,
  unsigned  int       n_channels
) {
  sa_stream_t   * s = 0;

  /*
   * Make sure we return a NULL stream pointer on failure.
   */
  if (_s == NULL) {
    return SA_ERROR_INVALID;
  }
  *_s = NULL;

  if (mode != SA_MODE_WRONLY) {
    return SA_ERROR_NOT_SUPPORTED;
  }
  if (format != SA_PCM_FORMAT_S16_NE) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  /*
   * Allocate the instance and required resources.
   */
  if ((s = malloc(sizeof(sa_stream_t))) == NULL) {
    return SA_ERROR_OOM;
  }

  s->output_unit  = NULL;
  s->bytes_written = 0;
  s->last_position = 0;
  s->rate         = rate;
  s->n_channels   = n_channels;
  s->pulseaudio   = 0;
  s->resumed      = 0;

  *_s = s;
  return SA_SUCCESS;
}


int
sa_stream_open(sa_stream_t *s) {
  snd_output_t* out;
  char* buf;
  size_t bufsz;
  snd_pcm_hw_params_t* hwparams;
  snd_pcm_sw_params_t* swparams;
  int dir;
  snd_pcm_uframes_t period;

  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (s->output_unit != NULL) {
    return SA_ERROR_INVALID;
  }

  pthread_mutex_lock(&sa_alsa_mutex);

  /* Turn off debug output to stderr */
  snd_lib_error_set_handler(quiet_error_handler);

  if (snd_pcm_open(&s->output_unit, 
                   "default", 
                   SND_PCM_STREAM_PLAYBACK, 
                   0) < 0) {
    pthread_mutex_unlock(&sa_alsa_mutex);
    return SA_ERROR_NO_DEVICE;
  }
  
  if (snd_pcm_set_params(s->output_unit,
#ifdef SA_LITTLE_ENDIAN
                         SND_PCM_FORMAT_S16_LE,
#else
                         SND_PCM_FORMAT_S16_BE,
#endif
                         SND_PCM_ACCESS_RW_INTERLEAVED,
                         s->n_channels,
                         s->rate,
                         1,
                         500000) < 0) {
    snd_pcm_close(s->output_unit);
    s->output_unit = NULL;
    pthread_mutex_unlock(&sa_alsa_mutex);
    return SA_ERROR_NOT_SUPPORTED;
  }
  
  /* ugly alsa-pulse plugin detection */
  snd_output_buffer_open(&out);
  snd_pcm_dump(s->output_unit, out);
  bufsz = snd_output_buffer_string(out, &buf);
  if (strncmp(buf, "ALSA <-> PulseAudio PCM I/O Plugin", bufsz) > 0 ) {
    s->pulseaudio = 1;
  }
  snd_output_close(out);

  snd_pcm_hw_params_alloca(&hwparams);
  snd_pcm_hw_params_current(s->output_unit, hwparams);
  snd_pcm_hw_params_get_period_size(hwparams, &period, &dir);

  pthread_mutex_unlock(&sa_alsa_mutex);

  return SA_SUCCESS;
}


int
sa_stream_get_min_write(sa_stream_t *s, size_t *size) {
  int r;
  snd_pcm_uframes_t threshold;
  snd_pcm_sw_params_t* swparams;
  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }
  snd_pcm_sw_params_alloca(&swparams);
  snd_pcm_sw_params_current(s->output_unit, swparams);
  r = snd_pcm_sw_params_get_start_threshold(swparams, &threshold);
  if (r < 0) {
    return SA_ERROR_NO_INIT;
  }
  *size = snd_pcm_frames_to_bytes(s->output_unit, threshold);

  return SA_SUCCESS;
}


int
sa_stream_destroy(sa_stream_t *s) {
  int result = SA_SUCCESS;

  if (s == NULL) {
    return result;
  }
  /*
   * Shut down the audio output device.
   */
  if (s->output_unit != NULL) {
    pthread_mutex_lock(&sa_alsa_mutex);
    if (snd_pcm_close(s->output_unit) < 0) {
      result = SA_ERROR_SYSTEM;
    }
    pthread_mutex_unlock(&sa_alsa_mutex);
  }
  free(s);
  return result;
}



/*
 * -----------------------------------------------------------------------------
 * Data read and write functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_write(sa_stream_t *s, const void *data, size_t nbytes) {
  snd_pcm_sframes_t frames, nframes, avail;

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (nbytes == 0) {
    return SA_SUCCESS;
  }

  nframes = snd_pcm_bytes_to_frames(s->output_unit, nbytes);
  while(nframes>0) {
    if (s->resumed) {
      avail = snd_pcm_avail_update(s->output_unit);
      frames = snd_pcm_writei(s->output_unit, data, nframes > avail ? avail : nframes);
      avail = snd_pcm_avail_update(s->output_unit);
      s->resumed = avail != 0;
    } else {
      avail = snd_pcm_avail_update(s->output_unit);
      avail = avail < 64 ? 64 : avail;
      frames = snd_pcm_writei(s->output_unit, data, nframes > avail ? avail : nframes);
    }
    if (frames < 0) {
      int r = snd_pcm_recover(s->output_unit, frames, 1);
      if (r < 0) {
        return SA_ERROR_SYSTEM;
      }
    } else {
      size_t bytes = snd_pcm_frames_to_bytes(s->output_unit, frames);
      nframes -= frames;
      data = ((unsigned char *)data) + bytes;
      s->bytes_written += bytes;
    }
  }

  return SA_SUCCESS;
}

/*
 * -----------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_get_write_size(sa_stream_t *s, size_t *size) {
  snd_pcm_sframes_t avail;

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  do {
    avail = snd_pcm_avail_update(s->output_unit);
    if (avail < 0) {
      int r = snd_pcm_recover(s->output_unit, avail, 1);
      if (r < 0) {
        return SA_ERROR_SYSTEM;
      }
      continue;
    }
    break;
  } while (1);

  *size = snd_pcm_frames_to_bytes(s->output_unit, avail);

  return SA_SUCCESS;
}

int
sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos) {
  snd_pcm_sframes_t delay;
  
  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  if (position != SA_POSITION_WRITE_SOFTWARE) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  if (snd_pcm_state(s->output_unit) != SND_PCM_STATE_RUNNING) {
    *pos = s->last_position;
    return SA_SUCCESS;
  }

  if (snd_pcm_delay(s->output_unit, &delay) != 0) {
    return SA_ERROR_SYSTEM;
  }

  /* delay means audio is 'x' frames behind what we've written. We need to
     subtract the delay from the data written to return the actual bytes played.

     due to buffering, the delay can be larger than the amount we've
     written--in this case, report position as zero. */
  *pos = s->bytes_written;
  if (*pos >= snd_pcm_frames_to_bytes(s->output_unit, delay)) {
    *pos -= snd_pcm_frames_to_bytes(s->output_unit, delay);
  } else {
    *pos = 0;
  }
  s->last_position = *pos;

  return SA_SUCCESS;
}


int
sa_stream_pause(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  if (snd_pcm_pause(s->output_unit, 1) != 0)
    return SA_ERROR_NOT_SUPPORTED;

  return SA_SUCCESS;
}


int
sa_stream_resume(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  if (s->pulseaudio) {
    s->resumed = 1;
  }

  if (snd_pcm_pause(s->output_unit, 0) != 0)
    return SA_ERROR_NOT_SUPPORTED;
  return SA_SUCCESS;
}


int
sa_stream_drain(sa_stream_t *s)
{
  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  if (snd_pcm_state(s->output_unit) == SND_PCM_STATE_PREPARED) {
    size_t min_samples = 0;
    size_t min_bytes = 0;
    void *buf;

    if (sa_stream_get_min_write(s, &min_samples) < 0)
      return SA_ERROR_SYSTEM;
    min_bytes = snd_pcm_frames_to_bytes(s->output_unit, min_samples);    

    buf = malloc(min_bytes);
    if (!buf)
      return SA_ERROR_SYSTEM;
    memset(buf, 0, min_bytes);
    sa_stream_write(s, buf, min_bytes);
    free(buf);
  }

  if (snd_pcm_state(s->output_unit) != SND_PCM_STATE_RUNNING) {
    return SA_ERROR_INVALID;
  }
  snd_pcm_drain(s->output_unit);
  return SA_SUCCESS;
}



/*
 * -----------------------------------------------------------------------------
 * Extension functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_set_volume_abs(sa_stream_t *s, float vol) {
  snd_mixer_t* mixer = 0;
  snd_mixer_elem_t* elem = 0;
  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  if (snd_mixer_open(&mixer, 0) < 0) {
    return SA_ERROR_SYSTEM;
  }

  if (snd_mixer_attach(mixer, "default") < 0) {
    snd_mixer_close(mixer);
    return SA_ERROR_SYSTEM;
  }

  if (snd_mixer_selem_register(mixer, NULL, NULL) < 0) {
    snd_mixer_close(mixer);
    return SA_ERROR_SYSTEM;
  }

  if (snd_mixer_load(mixer) < 0) {
    snd_mixer_close(mixer);
    return SA_ERROR_SYSTEM;
  }

#if 0
  snd_mixer_elem_t* elem = 0;
  for (elem = snd_mixer_first_elem(mixer); elem != NULL; elem = snd_mixer_elem_next(elem)) {
    if (snd_mixer_selem_has_playback_volume(elem)) {
      printf("Playback %s\n", snd_mixer_selem_get_name(elem));
    }
    else {
      printf("No Playback: %s\n", snd_mixer_selem_get_name(elem));
    }
  }
#endif
  elem = snd_mixer_first_elem(mixer);
  if (elem && snd_mixer_selem_has_playback_volume(elem)) {
    long min = 0;
    long max = 0;
    if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) >= 0) {
      snd_mixer_selem_set_playback_volume_all(elem, (max-min)*vol + min);
    } 
  }
  snd_mixer_close(mixer);

  return SA_SUCCESS;
}


int
sa_stream_get_volume_abs(sa_stream_t *s, float *vol) {
  snd_mixer_t* mixer = 0;
  snd_mixer_elem_t* elem = 0;
  long value = 0;

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  if (snd_mixer_open(&mixer, 0) < 0) {
    return SA_ERROR_SYSTEM;
  }

  if (snd_mixer_attach(mixer, "default") < 0) {
    snd_mixer_close(mixer);
    return SA_ERROR_SYSTEM;
  }

  if (snd_mixer_selem_register(mixer, NULL, NULL) < 0) {
    snd_mixer_close(mixer);
    return SA_ERROR_SYSTEM;
  }

  if (snd_mixer_load(mixer) < 0) {
    snd_mixer_close(mixer);
    return SA_ERROR_SYSTEM;
  }

  elem = snd_mixer_first_elem(mixer);
  if (elem && snd_mixer_selem_get_playback_volume(elem, 0, &value) >= 0) {
    long min = 0;
    long max = 0;
    if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) >= 0) {
      *vol = (float)(value-min)/(float)(max-min);
    } 
  }
  snd_mixer_close(mixer);

  return SA_SUCCESS;
}



/*
 * -----------------------------------------------------------------------------
 * Unsupported functions
 * -----------------------------------------------------------------------------
 */
#define UNSUPPORTED(func)   func { return SA_ERROR_NOT_SUPPORTED; }

UNSUPPORTED(int sa_stream_create_opaque(sa_stream_t **s, const char *client_name, sa_mode_t mode, const char *codec))
UNSUPPORTED(int sa_stream_set_write_lower_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_read_lower_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_write_upper_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_read_upper_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_channel_map(sa_stream_t *s, const sa_channel_t map[], unsigned int n))
UNSUPPORTED(int sa_stream_set_xrun_mode(sa_stream_t *s, sa_xrun_mode_t mode))
UNSUPPORTED(int sa_stream_set_non_interleaved(sa_stream_t *s, int enable))
UNSUPPORTED(int sa_stream_set_dynamic_rate(sa_stream_t *s, int enable))
UNSUPPORTED(int sa_stream_set_driver(sa_stream_t *s, const char *driver))
UNSUPPORTED(int sa_stream_start_thread(sa_stream_t *s, sa_event_callback_t callback))
UNSUPPORTED(int sa_stream_stop_thread(sa_stream_t *s))
UNSUPPORTED(int sa_stream_change_device(sa_stream_t *s, const char *device_name))
UNSUPPORTED(int sa_stream_change_read_volume(sa_stream_t *s, const int32_t vol[], unsigned int n))
UNSUPPORTED(int sa_stream_change_write_volume(sa_stream_t *s, const int32_t vol[], unsigned int n))
UNSUPPORTED(int sa_stream_change_rate(sa_stream_t *s, unsigned int rate))
UNSUPPORTED(int sa_stream_change_meta_data(sa_stream_t *s, const char *name, const void *data, size_t size))
UNSUPPORTED(int sa_stream_change_user_data(sa_stream_t *s, const void *value))
UNSUPPORTED(int sa_stream_set_adjust_rate(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_nchannels(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_pcm_format(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_watermarks(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_get_mode(sa_stream_t *s, sa_mode_t *access_mode))
UNSUPPORTED(int sa_stream_get_codec(sa_stream_t *s, char *codec, size_t *size))
UNSUPPORTED(int sa_stream_get_pcm_format(sa_stream_t *s, sa_pcm_format_t *format))
UNSUPPORTED(int sa_stream_get_rate(sa_stream_t *s, unsigned int *rate))
UNSUPPORTED(int sa_stream_get_nchannels(sa_stream_t *s, int *nchannels))
UNSUPPORTED(int sa_stream_get_user_data(sa_stream_t *s, void **value))
UNSUPPORTED(int sa_stream_get_write_lower_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_read_lower_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_write_upper_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_read_upper_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_channel_map(sa_stream_t *s, sa_channel_t map[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_xrun_mode(sa_stream_t *s, sa_xrun_mode_t *mode))
UNSUPPORTED(int sa_stream_get_non_interleaved(sa_stream_t *s, int *enabled))
UNSUPPORTED(int sa_stream_get_dynamic_rate(sa_stream_t *s, int *enabled))
UNSUPPORTED(int sa_stream_get_driver(sa_stream_t *s, char *driver_name, size_t *size))
UNSUPPORTED(int sa_stream_get_device(sa_stream_t *s, char *device_name, size_t *size))
UNSUPPORTED(int sa_stream_get_read_volume(sa_stream_t *s, int32_t vol[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_write_volume(sa_stream_t *s, int32_t vol[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_meta_data(sa_stream_t *s, const char *name, void*data, size_t *size))
UNSUPPORTED(int sa_stream_get_adjust_rate(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_nchannels(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_pcm_format(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_watermarks(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_state(sa_stream_t *s, sa_state_t *state))
UNSUPPORTED(int sa_stream_get_event_error(sa_stream_t *s, sa_error_t *error))
UNSUPPORTED(int sa_stream_get_event_notify(sa_stream_t *s, sa_notify_t *notify))
UNSUPPORTED(int sa_stream_read(sa_stream_t *s, void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_read_ni(sa_stream_t *s, unsigned int channel, void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_write_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_pwrite(sa_stream_t *s, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence))
UNSUPPORTED(int sa_stream_pwrite_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence))
UNSUPPORTED(int sa_stream_get_read_size(sa_stream_t *s, size_t *size))

const char *sa_strerror(int code) { return NULL; }

