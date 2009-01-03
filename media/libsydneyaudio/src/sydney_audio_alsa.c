/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is
 * CSIRO
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Michael Martin
 *                 Chris Double (chris.double@double.co.nz)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** *
 */
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "sydney_audio.h"

/* ALSA implementation based heavily on sydney_audio_mac.c */


struct sa_stream {
  snd_pcm_t*        output_unit;
  char              playing;
  int64_t           bytes_played;
  int64_t           bytes_written;

  /* audio format info */
  unsigned int      rate;
  unsigned int      n_channels;
  snd_pcm_uframes_t buffer_size;
  snd_pcm_uframes_t period_size;
  unsigned int      buffer_bytes;
  unsigned int      period_bytes;
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
  if (format != SA_PCM_FORMAT_S16_LE) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  /*
   * Allocate the instance and required resources.
   */
  if ((s = malloc(sizeof(sa_stream_t))) == NULL) {
    return SA_ERROR_OOM;
  }

  s->output_unit  = NULL;
  s->playing      = 0;
  s->bytes_played = 0;
  s->bytes_written = 0;
  s->rate         = rate;
  s->n_channels   = n_channels;

  *_s = s;
  return SA_SUCCESS;
}


int
sa_stream_open(sa_stream_t *s) {

  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (s->output_unit != NULL) {
    return SA_ERROR_INVALID;
  }

  /* Turn off debug output to stderr */
  snd_lib_error_set_handler(quiet_error_handler);

  if (snd_pcm_open(&s->output_unit, 
                   "default", 
                   SND_PCM_STREAM_PLAYBACK, 
                   0) < 0) {
    return SA_ERROR_NO_DEVICE;
  }
  
  if (snd_pcm_set_params(s->output_unit,
                         SND_PCM_FORMAT_S16_LE,
                         SND_PCM_ACCESS_RW_INTERLEAVED,
                         s->n_channels,
                         s->rate,
                         1,
                         500000) < 0) {
    snd_pcm_close(s->output_unit);
    s->output_unit = NULL;
    return SA_ERROR_NOT_SUPPORTED;
  }
  
  if (snd_pcm_get_params(s->output_unit, &s->buffer_size, &s->period_size) < 0) {
    snd_pcm_close(s->output_unit);
    s->output_unit = NULL;
    return SA_ERROR_NOT_SUPPORTED;
  }
  s->period_bytes = (unsigned int)snd_pcm_frames_to_bytes(s->output_unit, s->period_size);
  s->buffer_bytes = (unsigned int)snd_pcm_frames_to_bytes(s->output_unit, s->buffer_size);

  return SA_SUCCESS;
}


int
sa_stream_destroy(sa_stream_t *s) {
  int result = SA_SUCCESS;

  if (s == NULL) {
    return SA_SUCCESS;
  }
  /*
   * Shut down the audio output device.
   */
  if (s->output_unit != NULL) {
    if (s->playing && snd_pcm_close(s->output_unit) < 0) {
      result = SA_ERROR_SYSTEM;
    }
  }
  return result;
}



/*
 * -----------------------------------------------------------------------------
 * Data read and write functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_write(sa_stream_t *s, const void *data, size_t nbytes) {
  int result = SA_SUCCESS;
  snd_pcm_sframes_t frames, nframes;

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (nbytes == 0) {
    return SA_SUCCESS;
  }

  nframes = snd_pcm_bytes_to_frames(s->output_unit, nbytes);

  while(nframes>0) {
    snd_pcm_sframes_t len = s->period_size;
    if(nframes < s->period_size)
      len = nframes;
    frames = snd_pcm_writei(s->output_unit, data, len);
    if (frames < 0) {
      frames = snd_pcm_recover(s->output_unit, frames, 1);
      if (frames < 0) {
        printf("snc_pcm_recover error: %s\n", snd_strerror(frames));
        return SA_ERROR_SYSTEM;
      }
      if(frames > 0 && frames < len)
        printf("short write (expected %d, wrote %d)\n", (int)len, (int)frames);
    }
    nframes -= s->period_size;
    data = ((unsigned char *)data) + s->period_bytes;
  }

  s->bytes_written += nbytes;

  if (!s->playing) {
    s->playing = 1;
  }

  return result;
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

  avail = snd_pcm_avail_update(s->output_unit);

  *size = snd_pcm_frames_to_bytes(s->output_unit, avail);

  return SA_SUCCESS;
}

int
sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos) {
  int err;
  snd_pcm_sframes_t delay;
  
  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  if (position != SA_POSITION_WRITE_SOFTWARE) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  if (snd_pcm_state(s->output_unit) != SND_PCM_STATE_RUNNING) {
    delay = 0;
  }
  else if (snd_pcm_delay (s->output_unit, &delay) != 0) {
    printf("snd_pcm_delay failed\n");
    delay = 0;
  }
  if (delay < 0) {
    delay = 0;
  }
  if (delay > s->buffer_size) {
    printf("delay just wrong %d vs %d\n", delay, s->buffer_size);
    delay = s->buffer_size;
  }

  s->bytes_played = s->bytes_written - snd_pcm_frames_to_bytes(s->output_unit, delay);

  *pos = s->bytes_played;

  return SA_SUCCESS;
}


int
sa_stream_pause(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }
#if 0 /* TODO */
  AudioOutputUnitStop(s->output_unit);
#endif

  return SA_SUCCESS;
}


int
sa_stream_resume(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  /*
   * The audio device resets its mSampleTime counter after pausing,
   * so we need to clear our tracking value to keep that in sync.
   */
  s->bytes_played = s->bytes_written = 0;

  return SA_SUCCESS;
}


int
sa_stream_drain(sa_stream_t *s)
{
  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
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

