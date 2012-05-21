/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include "sydney_audio.h"

// for versions newer than 3.6.1
#define OSS_VERSION(x, y, z) (x << 16 | y << 8 | z)
// support only versions newer than 3.6.1
#define SUPP_OSS_VERSION OSS_VERSION(3,0,1)

#if (SOUND_VERSION < SUPP_OSS_VERSION)
#error Unsupported OSS Version
#else

typedef struct sa_buf sa_buf;
struct sa_buf {
  unsigned int      size;
  unsigned int      start;
  unsigned int      end;
  sa_buf          * next;
  unsigned char     data[0];
};

struct sa_stream {
  char*                output_unit;
  int                output_fd;
  pthread_t         thread_id;
  pthread_mutex_t   mutex;
  char              playing;
  int64_t           bytes_played;

  /* audio format info */
  unsigned int      rate;
  unsigned int      channels;
  int               format;

  /* buffer list */
  sa_buf          * bl_head;
  sa_buf          * bl_tail;
  int               n_bufs;
};


/*
 * Use a default buffer size with enough room for one second of audio,
 * assuming stereo data at 44.1kHz with 32 bits per channel, and impose
 * a generous limit on the number of buffers.
 */
#define BUF_SIZE    (2 * 44100 * 4)
#define BUF_LIMIT   5

#if BUF_LIMIT < 2
#error BUF_LIMIT must be at least 2!
#endif

static void audio_callback(void* s);
static sa_buf *new_buffer(void);

/** Private functions - implementation specific */

/*!
 * \brief private function mapping Sudney Audio format to OSS formats
 * \param format - Sydney Audio API specific format
 * \param - filled by the function with a value for corresponding OSS format
 * \return - Sydney API error value as in ::sa_pcm_format_t
 * */
static int oss_audio_format(sa_pcm_format_t sa_format, int *fmt) {
    *fmt = -1;
    switch (sa_format) {
        case SA_PCM_FORMAT_U8:
            *fmt = AFMT_U8;
            break;
        case SA_PCM_FORMAT_ULAW:
            *fmt = AFMT_MU_LAW;
            break;
        case SA_PCM_FORMAT_ALAW:
            *fmt = AFMT_A_LAW;
            break;
        /* 16-bit little endian (LE) format */
        case SA_PCM_FORMAT_S16_LE:
            *fmt = AFMT_S16_LE;
            break;
        /* 16-bit big endian (BE) format */
        case SA_PCM_FORMAT_S16_BE:
            *fmt = AFMT_S16_BE;
            break;
#if SOUND_VERSION >= OSS_VERSION(4,0,0)
        /* 24-bit formats (LSB aligned in 32 bit word) */
        case SA_PCM_FORMAT_S24_LE:
            *fmt = AFMT_S24_LE;
            break;
        /* 24-bit formats (LSB aligned in 32 bit word) */
        case SA_PCM_FORMAT_S24_BE:
            *fmt = AFMT_S24_BE;
            break;
        /* 32-bit format little endian */
        case SA_PCM_FORMAT_S32_LE:
            *fmt = AFMT_S32_LE;
            break;
        /* 32-bit format big endian */
        case SA_PCM_FORMAT_S32_BE:
            *fmt = AFMT_S32_BE;
            break;
#endif
        default:
            return SA_ERROR_NOT_SUPPORTED;
            break;
    }
    return SA_SUCCESS;
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
  unsigned  int       channels
) {
  sa_stream_t * s = 0;
  int fmt = 0;

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
  if (oss_audio_format(format, &fmt) != SA_SUCCESS) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  /*
   * Allocate the instance and required resources.
   */
  if ((s = malloc(sizeof(sa_stream_t))) == NULL) {
    return SA_ERROR_OOM;
  }
  if ((s->bl_head = new_buffer()) == NULL) {
    free(s);
    return SA_ERROR_OOM;
  }
  if (pthread_mutex_init(&s->mutex, NULL) != 0) {
    free(s->bl_head);
    free(s);
    return SA_ERROR_SYSTEM;
  }

  s->output_unit  = "/dev/dsp";
  s->output_fd    = -1;
  s->thread_id    = 0;
  s->playing      = 0;
  s->bytes_played = 0;
  s->rate         = rate;
  s->channels     = channels;
  s->format       = fmt;
  s->bl_tail      = s->bl_head;
  s->n_bufs       = 1;

  *_s = s;
  return SA_SUCCESS;
}


int
sa_stream_open(sa_stream_t *s) {
  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (s->output_unit == NULL || s->output_fd != -1) {
    return SA_ERROR_INVALID;
  }

  // open the default OSS device
  if ((s->output_fd = open(s->output_unit, O_WRONLY, 0)) == -1) {
    return SA_ERROR_NO_DEVICE;
  }

  // set the playback rate
  if (ioctl(s->output_fd, SNDCTL_DSP_SPEED, &(s->rate)) < 0) {
    close(s->output_fd);
    s->output_fd = -1;
    return SA_ERROR_NOT_SUPPORTED;
  }

  // set the channel numbers
  if (ioctl(s->output_fd, SNDCTL_DSP_CHANNELS, &(s->channels)) < 0) {
    close(s->output_fd);
    s->output_fd = -1;
    return SA_ERROR_NOT_SUPPORTED;
  }

  if (ioctl(s->output_fd, SNDCTL_DSP_SETFMT, &(s->format)) < 0 ) {
    close(s->output_fd);
    s->output_fd = -1;
    return SA_ERROR_NOT_SUPPORTED;
  }

  return SA_SUCCESS;
}


int
sa_stream_destroy(sa_stream_t *s) {
  int result = SA_SUCCESS;
  pthread_t thread_id;

  if (s == NULL) {
    return SA_SUCCESS;
  }

  pthread_mutex_lock(&s->mutex);

  thread_id = s->thread_id;

  /*
   * This causes the thread sending data to OSS to stop
   */
  s->thread_id = 0;

  /*
   * Shut down the audio output device.
   */
  if (s->output_fd != -1) {
    if (s->playing && close(s->output_fd) < 0) {
      result = SA_ERROR_SYSTEM;
    }
  }

  pthread_mutex_unlock(&s->mutex);

  pthread_join(thread_id, NULL);

  /*
   * Release resources.
   */
  if (pthread_mutex_destroy(&s->mutex) != 0) {
    result = SA_ERROR_SYSTEM;
  }
  while (s->bl_head != NULL) {
    sa_buf  * next = s->bl_head->next;
    free(s->bl_head);
    s->bl_head = next;
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
  int result = SA_SUCCESS;

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (nbytes == 0) {
    return SA_SUCCESS;
  }

  pthread_mutex_lock(&s->mutex);

  /*
   * Append the new data to the end of our buffer list.
   */
  while (1) {
    unsigned int avail = s->bl_tail->size - s->bl_tail->end;

    if (nbytes <= avail) {

      /*
       * The new data will fit into the current tail buffer, so
       * just copy it in and we're done.
       */
      memcpy(s->bl_tail->data + s->bl_tail->end, data, nbytes);
      s->bl_tail->end += nbytes;
      break;

    } else {

      /*
       * Copy what we can into the tail and allocate a new buffer
       * for the rest.
       */
      memcpy(s->bl_tail->data + s->bl_tail->end, data, avail);
      s->bl_tail->end += avail;
      data = ((unsigned char *)data) + avail;
      nbytes -= avail;

      /*
       * If we still have data left to copy but we've hit the limit of
       * allowable buffer allocations, we need to spin for a bit to allow
       * the audio callback function to slurp some more data up.
       */
      if (nbytes > 0 && s->n_bufs == BUF_LIMIT) {
#ifdef TIMING_TRACE
        printf("#");  /* too much audio data */
#endif
        if (!s->playing) {
          /*
           * We haven't even started playing yet! That means the
           * BUF_SIZE/BUF_LIMIT values are too low... Not much we can
           * do here; spinning won't help because the audio callback
           * hasn't been enabled yet. Oh well, error time.
           */
          printf("Too much audio data received before audio device enabled!\n");
          result = SA_ERROR_SYSTEM;
          break;
        }
        while (s->n_bufs == BUF_LIMIT) {
          struct timespec ts = {0, 1000000};
          pthread_mutex_unlock(&s->mutex);
          nanosleep(&ts, NULL);
          pthread_mutex_lock(&s->mutex);
        }
      }

      /*
       * Allocate a new tail buffer, and go 'round again to fill it up.
       */
      if ((s->bl_tail->next = new_buffer()) == NULL) {
        result = SA_ERROR_OOM;
        break;
      }
      s->n_bufs++;
      s->bl_tail = s->bl_tail->next;

    } /* if (nbytes <= avail), else */

  } /* while (1) */

  pthread_mutex_unlock(&s->mutex);

  /*
   * Once we have our first block of audio data, enable the audio callback
   * function. This doesn't need to be protected by the mutex, because
   * s->playing is not used in the audio callback thread, and it's probably
   * better not to be inside the lock when we enable the audio callback.
   */
  if (!s->playing) {
    s->playing = 1;
    if (pthread_create(&s->thread_id, NULL, (void *)audio_callback, s) != 0) {
      result = SA_ERROR_SYSTEM;
    }
  }

  return result;
}


static void audio_callback(void* data)
{
  sa_stream_t* s = (sa_stream_t*)data;
  audio_buf_info info;
  char* buffer = 0;
  unsigned int avail;
  int frames;

#ifdef TIMING_TRACE
  printf(".");  /* audio read 'tick' */
#endif

  ioctl(s->output_fd, SNDCTL_DSP_GETOSPACE, &info);
  buffer = malloc(info.bytes);

  while(1) {
    char* dst = buffer;
    unsigned int bytes_to_copy = info.bytes;
    int bytes = info.bytes;

    pthread_mutex_lock(&s->mutex);
    if (!s->thread_id)
      break;

    /*
     * Consume data from the start of the buffer list.
     */
    while (1) {
      assert(s->bl_head->start <= s->bl_head->end);
      avail = s->bl_head->end - s->bl_head->start;

      if (avail >= bytes_to_copy) {
        /*
         * We have all we need in the head buffer, so just grab it and go.
         */
        memcpy(dst, s->bl_head->data + s->bl_head->start, bytes_to_copy);
        s->bl_head->start += bytes_to_copy;
        s->bytes_played += bytes_to_copy;
        break;

      } else {

        sa_buf* next = 0;
        /*
         * Copy what we can from the head and move on to the next buffer.
         */
        memcpy(dst, s->bl_head->data + s->bl_head->start, avail);
        s->bl_head->start += avail;
        dst += avail;
        bytes_to_copy -= avail;
        s->bytes_played += avail;

        /*
         * We want to free the now-empty buffer, but not if it's also the
         * current tail. If it is the tail, we don't have enough data to fill
         * the destination buffer, so we write less and give up.
         */
        next = s->bl_head->next;
        if (next == NULL) {
#ifdef TIMING_TRACE
          printf("!");  /* not enough audio data */
#endif
          bytes = bytes-bytes_to_copy;
          break;
        }
        free(s->bl_head);
        s->bl_head = next;
        s->n_bufs--;

      } /* if (avail >= bytes_to_copy), else */

    } /* while (1) */

    pthread_mutex_unlock(&s->mutex);

    if(bytes > 0) {
      frames = write(s->output_fd, buffer, bytes);
      if (frames < 0) {
          printf("error writing to sound device\n");
      }
      if (frames >= 0 && frames != bytes) {
         printf("short write (expected %d, wrote %d)\n", (int)bytes, (int)frames);
      }
    }
  }
  free(buffer);
}



/*
 * -----------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_get_write_size(sa_stream_t *s, size_t *size) {
  sa_buf  * b;
  size_t    used = 0;

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  pthread_mutex_lock(&s->mutex);

  /*
   * Sum up the used portions of our buffers and subtract that from
   * the pre-defined max allowed allocation.
   */
  for (b = s->bl_head; b != NULL; b = b->next) {
    used += b->end - b->start;
  }
  *size = BUF_SIZE * BUF_LIMIT - used;

  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}


int
sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos) {
   int err;
   count_info ptr;

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (position != SA_POSITION_WRITE_SOFTWARE) {
    return SA_ERROR_NOT_SUPPORTED;
  }
  if ((err = ioctl(s->output_fd, 
                       SNDCTL_DSP_GETOPTR, 
                       &ptr)) <0) {
      fprintf(stderr, "Error reading playback position\n");
      return SA_ERROR_OOM;
  }

  pthread_mutex_lock(&s->mutex);
  *pos = (int64_t)ptr.bytes;
  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}


int
sa_stream_pause(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  pthread_mutex_lock(&s->mutex);
#if 0 /* TODO */
  AudioOutputUnitStop(s->output_unit);
#endif
  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}


int
sa_stream_resume(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  pthread_mutex_lock(&s->mutex);

  /*
   * The audio device resets its mSampleTime counter after pausing,
   * so we need to clear our tracking value to keep that in sync.
   */
  s->bytes_played = 0;
#if 0 /* TODO */
  AudioOutputUnitStart(s->output_unit);
#endif
  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}


static sa_buf *
new_buffer(void) {
  sa_buf  * b = malloc(sizeof(sa_buf) + BUF_SIZE);
  if (b != NULL) {
    b->size  = BUF_SIZE;
    b->start = 0;
    b->end   = 0;
    b->next  = NULL;
  }
  return b;
}



/*
 * -----------------------------------------------------------------------------
 * Extension functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_set_volume_abs(sa_stream_t *s, float vol) {
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }
#if SOUND_VERSION >= OSS_VERSION(4,0,0)
  int mvol = ((int)(100*vol)) | ((int)(100*vol) << 8);
  if (ioctl(s->output_fd, SNDCTL_DSP_SETPLAYVOL, &mvol) < 0){
    return SA_ERROR_SYSTEM;
  }
  return SA_SUCCESS;
#else
  return SA_ERROR_NOT_SUPPORTED;
#endif
}


int
sa_stream_get_volume_abs(sa_stream_t *s, float *vol) {

  if (vol == NULL) {
    return SA_ERROR_INVALID;
  }
  *vol = 0.0f;
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }
#if SOUND_VERSION >= OSS_VERSION(4,0,0)
  int mvol;
  if (ioctl(s->output_fd, SNDCTL_DSP_SETPLAYVOL, &mvol) < 0){
    return SA_ERROR_SYSTEM;
  }
  *vol = ((mvol & 0xFF) + (mvol >> 8)) / 200.0f;
  return SA_SUCCESS;
#else
  return SA_ERROR_NOT_SUPPORTED;
#endif
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
UNSUPPORTED(int sa_stream_drain(sa_stream_t *s))
UNSUPPORTED(int sa_stream_get_min_write(sa_stream_t *s, size_t *size))

const char *sa_strerror(int code) { return NULL; }

#endif
