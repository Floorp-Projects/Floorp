#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <unistd.h>
#include <sys/audio.h>
#include <sys/stat.h>
#include <sys/mixer.h>
#include "sydney_audio.h"

/* Sun Audio implementation based heavily on sydney_audio_mac.c */

#define DEFAULT_AUDIO_DEVICE "/dev/audio"
#define DEFAULT_DSP_DEVICE   "/dev/dsp"

/* Macros copied from audio_oss.h */
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (C) 4Front Technologies 1996-2008.
 *
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#define OSSIOCPARM_MASK 0x1fff          /* parameters must be < 8192 bytes */
#define OSSIOC_VOID     0x00000000      /* no parameters */
#define OSSIOC_OUT      0x20000000      /* copy out parameters */
#define OSSIOC_IN       0x40000000      /* copy in parameters */
#define OSSIOC_INOUT    (OSSIOC_IN|OSSIOC_OUT)
#define OSSIOC_SZ(t)    ((sizeof (t) & OSSIOCPARM_MASK) << 16)
#define __OSSIO(x, y)           ((int)(OSSIOC_VOID|(x<<8)|y))
#define __OSSIOR(x, y, t)       ((int)(OSSIOC_OUT|OSSIOC_SZ(t)|(x<<8)|y))
#define __OSSIOWR(x, y, t)      ((int)(OSSIOC_INOUT|OSSIOC_SZ(t)|(x<<8)|y))
#define SNDCTL_DSP_SPEED        __OSSIOWR('P', 2, int)
#define SNDCTL_DSP_CHANNELS     __OSSIOWR('P', 6, int)
#define SNDCTL_DSP_SETFMT       __OSSIOWR('P', 5, int)  /* Selects ONE fmt */
#define SNDCTL_DSP_GETPLAYVOL   __OSSIOR('P', 24, int)
#define SNDCTL_DSP_SETPLAYVOL   __OSSIOWR('P', 24, int)
#define SNDCTL_DSP_HALT_OUTPUT  __OSSIO('P', 34)
#define AFMT_S16_LE     0x00000010
#define AFMT_S16_BE     0x00000020
#ifdef SA_LITTLE_ENDIAN
#define AFMT_S16_NE    AFMT_S16_LE
#else
#define AFMT_S16_NE    AFMT_S16_BE
#endif

typedef struct sa_buf sa_buf;
struct sa_buf {
  unsigned int      size;
  unsigned int      start;
  unsigned int      end;
  sa_buf          * next;
  unsigned char     data[];
};

struct sa_stream {
  bool              using_oss;
  int               output_fd;
  pthread_t         thread_id;
  pthread_mutex_t   mutex;
  bool              playing;
  int64_t           bytes_played;

  /* audio format info */
  unsigned int      rate;
  unsigned int      n_channels;
  unsigned int      bytes_per_ch;

  /* buffer list */
  sa_buf          * bl_head;
  sa_buf          * bl_tail;
  int               n_bufs;
};

/* Use a default buffer size with enough room for one second of audio,
 * assuming stereo data at 44.1kHz with 32 bits per channel, and impose
 * a generous limit on the number of buffers.
 */
#define BUF_SIZE    (2 * 44100 * 4)
#define BUF_LIMIT   5

#if BUF_LIMIT < 2
#error BUF_LIMIT must be at least 2!
#endif

static void *audio_callback(void *s);
static sa_buf *new_buffer(void);
static int shutdown_device(sa_stream_t *s);

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
  sa_stream_t *s;
  if ((s = malloc(sizeof(sa_stream_t))) == NULL) {
    return SA_ERROR_OOM;
  }
  if ((s->bl_head = new_buffer()) == NULL) {
    free(s);
    return SA_ERROR_SYSTEM;
  }
  if (pthread_mutex_init(&s->mutex, NULL) != 0) {
    free(s->bl_head);
    free(s);
    return SA_ERROR_SYSTEM;
  }

  s->output_fd          = -1;
  s->playing            = false;
  s->bytes_played       = 0;
  s->rate               = rate;
  s->n_channels         = n_channels;
  s->bytes_per_ch       = 2;
  s->bl_tail            = s->bl_head;
  s->n_bufs             = 1;

  *_s = s;
  return SA_SUCCESS;
}

int
sa_stream_open(sa_stream_t *s) {
  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (s->output_fd != -1) {
    return SA_ERROR_INVALID;
  }

  /*
   * Open the default audio output unit.
   */

  /* If UTAUDIODEV is set, use it with Sun Audio interface */
  char * sa_device_name = getenv("UTAUDIODEV");
  char * dsp_device_name = NULL;
  if (!sa_device_name) {
    dsp_device_name = getenv("AUDIODSP");
    if (!dsp_device_name) {
      dsp_device_name = DEFAULT_DSP_DEVICE;
    }
    sa_device_name = getenv("AUDIODEV");
    if (!sa_device_name) {
      sa_device_name = DEFAULT_AUDIO_DEVICE;
    }
  }

  int fd = -1;
  s->using_oss = false;
  /* Try to use OSS if available */
  if (dsp_device_name) {
    fd = open(dsp_device_name, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
      s->using_oss = true;
    }
  }

  /* Try Sun Audio */
  if (!s->using_oss) {
    fd = open(sa_device_name, O_WRONLY | O_NONBLOCK);
  }

  if (fd < 0)
  {
    printf("Open %s failed:%s.\n", sa_device_name, strerror(errno));
    return SA_ERROR_NO_DEVICE;
  }

  if (s->using_oss) {
    /* set the playback rate */
    if (ioctl(fd, SNDCTL_DSP_SPEED, &(s->rate)) < 0) {
      close(fd);
      return SA_ERROR_NOT_SUPPORTED;
    }

    /* set the channel numbers */
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &(s->n_channels)) < 0) {
      close(fd);
      return SA_ERROR_NOT_SUPPORTED;
    }

    int format = AFMT_S16_NE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) < 0)  {
      close(fd);
      return SA_ERROR_NOT_SUPPORTED;
    }

    s->output_fd = fd;
    return SA_SUCCESS;
  }

  audio_info_t audio_info;
  AUDIO_INITINFO(&audio_info)
  audio_info.play.sample_rate = s->rate;
  audio_info.play.channels    = s->n_channels;
  audio_info.play.precision   = s->bytes_per_ch * 8;

  /* Signed Linear PCM encoding */
  audio_info.play.encoding = AUDIO_ENCODING_LINEAR;

  if (ioctl(fd, AUDIO_SETINFO, &audio_info) == -1) {
    printf("ioctl AUDIO_SETINFO failed.\n");
    close(fd);
    return SA_ERROR_NOT_SUPPORTED;
  }

  s->output_fd = fd;
  return SA_SUCCESS;
}

int
sa_stream_destroy(sa_stream_t *s) {
  if (s == NULL) {
    return SA_SUCCESS;
  }

  /*
   * Join the thread.
   */
  bool thread_created = false;
  pthread_mutex_lock(&s->mutex);
  if (s->playing) {
    thread_created = true;
    s->playing = false;
  }
  pthread_mutex_unlock(&s->mutex);
  if (thread_created) {
    pthread_join(s->thread_id, NULL);
  }

  int result = SA_SUCCESS;


  /*
   * Shutdown the audio output device.
   */
  result = shutdown_device(s);

  /*
   * Release resouces.
   */
  if (pthread_mutex_destroy(&s->mutex) != 0) {
    result = SA_ERROR_SYSTEM;
  }
  while (s->bl_head != NULL) {
    sa_buf * next = s->bl_head->next;
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
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }
  if (nbytes == 0) {
    return SA_SUCCESS;
  }

  pthread_mutex_lock(&s->mutex);

  /*
   * Append the new data to the end of our buffer list.
   */
  int result = SA_SUCCESS;
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
          pthread_mutex_unlock(&s->mutex);
          struct timespec ts = {0, 1000000};
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

  /*
   * Once we have our first block of audio data, enable the audio callback
   * function.
   */
  if (!s->playing) {
    s->playing = true;
    if (pthread_create(&s->thread_id, NULL, audio_callback, s) != 0) {
      result = SA_ERROR_SYSTEM;
    }
  }

  pthread_mutex_unlock(&s->mutex);

  return result;
}

static void *
audio_callback(void *data) {
  sa_stream_t *s = data;

  pthread_mutex_lock(&s->mutex);
  while (s->playing) {
    /*
     * Consume data from the start of the buffer list.
     */
    while (s->output_fd != -1) {
      unsigned int avail = s->bl_head->end - s->bl_head->start;
      if (avail > 0) {
        int written = write(s->output_fd, s->bl_head->data + s->bl_head->start, avail);
        if (written == -1) {
          break; /* Try again later. */
        }
        s->bl_head->start += written;
        s->bytes_played += written;
        if (written < avail) {
          break;
        }
      }

      sa_buf  * next = s->bl_head->next;
      if (next == NULL) {
#ifdef TIMING_TRACE
        printf("!");  /* not enough audio data */
#endif
        break;
      }
      free(s->bl_head);
      s->bl_head = next;
      s->n_bufs--;
    } /* while (s->output_fd != -1) */
    pthread_mutex_unlock(&s->mutex);
    struct timespec ts = {0, 1000000};
    nanosleep(&ts, NULL);
    pthread_mutex_lock(&s->mutex);
  } /* s->playing */
  pthread_mutex_unlock(&s->mutex);
  return NULL;
}

/*
 * -----------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_get_write_size(sa_stream_t *s, size_t *size) {
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }

  pthread_mutex_lock(&s->mutex);

  /*
   * The sum of the free space in the tail buffer plus the size of any new
   * buffers represents the write space available before blocking.
   */

  unsigned int avail = s->bl_tail->size - s->bl_tail->end;
  avail += (BUF_LIMIT - s->n_bufs) * BUF_SIZE;
  *size = avail;

  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos) {
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }
  if (position != SA_POSITION_WRITE_SOFTWARE) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  pthread_mutex_lock(&s->mutex);
  *pos = s->bytes_played;
  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}

int
sa_stream_drain(sa_stream_t *s) {
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }

  while (1) {
    pthread_mutex_lock(&s->mutex);
    sa_buf  * b;
    size_t    used = 0;
    for (b = s->bl_head; b != NULL; b = b->next) {
      used += b->end - b->start;
    }
    pthread_mutex_unlock(&s->mutex);

    if (used == 0) {
      break;
    }

    struct timespec ts = {0, 1000000};
    nanosleep(&ts, NULL);
  }
  return SA_SUCCESS;
}

int
sa_stream_pause(sa_stream_t *s) {
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }

  pthread_mutex_lock(&s->mutex);
  int result = shutdown_device(s);
  if (result == SA_SUCCESS) {
    s->output_fd = -1;
  }
  pthread_mutex_unlock(&s->mutex);

  return result;
}


int
sa_stream_resume(sa_stream_t *s) {
  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }

  pthread_mutex_lock(&s->mutex);
  int result = sa_stream_open(s);
  pthread_mutex_unlock(&s->mutex);

  return result;
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

static int
shutdown_device(sa_stream_t *s) {
  if (s->output_fd != -1)
  {
    /* Flush buffer. */
    if (s->using_oss) {
      ioctl(s->output_fd, SNDCTL_DSP_HALT_OUTPUT);
    } else {
      ioctl(s->output_fd, I_FLUSH);
    }

    if (close(s->output_fd) < 0)
    {
      return SA_ERROR_SYSTEM;
    }
  }
  return SA_SUCCESS;
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

  if (s->using_oss) {
    int mvol = ((int)(100 * vol)) | ((int)(100 * vol) << 8);
    if (ioctl(s->output_fd, SNDCTL_DSP_SETPLAYVOL, &mvol) < 0) {
      return SA_ERROR_SYSTEM;
    }
    return SA_SUCCESS;
  }

  unsigned int newVolume = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * vol + AUDIO_MIN_GAIN;

  /* Check if the new volume is valid or not */
  if ( newVolume < AUDIO_MIN_GAIN || newVolume > AUDIO_MAX_GAIN )
    return SA_ERROR_INVALID;

  pthread_mutex_lock(&s->mutex);
  audio_info_t audio_info;
  AUDIO_INITINFO(&audio_info);
  audio_info.play.gain = newVolume;
  int err = ioctl(s->output_fd, AUDIO_SETINFO, &audio_info);
  pthread_mutex_unlock(&s->mutex);

  if (err == -1)
  {
    perror("sa_stream_set_volume_abs failed\n");
    return SA_ERROR_SYSTEM;
  }

  return SA_SUCCESS;
}

int
sa_stream_get_volume_abs(sa_stream_t *s, float *vol) {
  if (s == NULL || s->output_fd == -1) {
    return SA_ERROR_NO_INIT;
  }

  if (s->using_oss) {
    int mvol;
    if (ioctl(s->output_fd, SNDCTL_DSP_GETPLAYVOL, &mvol) < 0){
      return SA_ERROR_SYSTEM;
    }
    *vol = ((mvol & 0xFF) + (mvol >> 8)) / 200.0f;
    return SA_SUCCESS;
  }

  pthread_mutex_lock(&s->mutex);
  audio_info_t audio_info;
  AUDIO_INITINFO(&audio_info);
  int err = ioctl(s->output_fd, AUDIO_GETINFO, &audio_info);
  pthread_mutex_unlock(&s->mutex);

  if (err == -1)
  {
    perror("sa_stream_get_volume_abs failed\n");
    return SA_ERROR_SYSTEM;
  }

  *vol =  (float)((audio_info.play.gain - AUDIO_MIN_GAIN))/(AUDIO_MAX_GAIN - AUDIO_MIN_GAIN);

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
UNSUPPORTED(int sa_stream_get_min_write(sa_stream_t *s, size_t *size))

const char *sa_strerror(int code) { return NULL; }

