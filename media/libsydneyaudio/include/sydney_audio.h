#ifndef foosydneyhfoo
#define foosydneyhfoo
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

/* Requirements:

- In sync mode, the device will automatically write data so that an initial read causes writes
of zeros to be issued to that one can do "while (1); {read(); write()}

- All functions are thread-safe and can be called in any thread context. None of the functions is
async-signal safe.
  
- It is assumed that duplex streams have a single clock (synchronised)
*/

#include <sys/types.h>
#if !defined (WIN32)
#include <sys/param.h>
#include <inttypes.h>
#if defined(__FreeBSD__)
#include <sys/endian.h>
#endif
#else
#include <stddef.h>
#endif

/* Detect byte order, based on sys/param.h */
#undef SA_LITTLE_ENDIAN
#undef SA_BIG_ENDIAN

#if defined(__BYTE_ORDER)
#   if __BYTE_ORDER == __LITTLE_ENDIAN
#       define SA_LITTLE_ENDIAN 1
#   elif __BYTE_ORDER == __BIG_ENDIAN
#       define SA_BIG_ENDIAN 1
#   endif
#elif defined(_BYTE_ORDER)
#   if _BYTE_ORDER == _LITTLE_ENDIAN
#       define SA_LITTLE_ENDIAN 1
#   elif _BYTE_ORDER == _BIG_ENDIAN
#       define SA_BIG_ENDIAN 1
#   endif
#elif defined(WIN32)
#   define SA_LITTLE_ENDIAN 1
#elif defined(__APPLE__)
#   if defined(__BIG_ENDIAN__)
#       define SA_BIG_ENDIAN 1
#   else
#       define SA_LITTLE_ENDIAN 1
#   endif
#elif defined(SOLARIS)
#   if defined(_BIG_ENDIAN)
#       define SA_BIG_ENDIAN 1
#   else
#       define SA_LITTLE_ENDIAN 1
#   endif
#elif defined(AIX)
#	define SA_BIG_ENDIAN 1
#else
#    error "Cannot determine byte order!"
#endif

#if defined(WIN32)
#if !defined(int32_t)
typedef __int32 int32_t;
#endif
#if !defined(int64_t)
typedef __int64 int64_t;
#endif
#endif

typedef struct sa_stream sa_stream_t;

#if defined(WIN32) || defined(OS2)
/* (left << 16 | right) (16 bits per channel) */
#define SA_VOLUME_MUTED ((int32_t) (0x00000000))
#else
/** Volume that corresponds to muted in/out */
#define SA_VOLUME_MUTED ((int32_t) (-0x80000000))
#endif

/** Ways to express seek offsets for pread/pwrite */
typedef enum {
    SA_SEEK_RELATIVE,
    SA_SEEK_ABSOLUTE,
    SA_SEEK_RELATIVE_END,
    _SA_SEEK_MAX
} sa_seek_t;

/** Supported formats */
typedef enum {
    SA_PCM_FORMAT_U8,
    SA_PCM_FORMAT_ULAW,
    SA_PCM_FORMAT_ALAW,
    SA_PCM_FORMAT_S16_LE,
    SA_PCM_FORMAT_S16_BE,
    SA_PCM_FORMAT_S24_LE,
    SA_PCM_FORMAT_S24_BE,
    SA_PCM_FORMAT_S32_LE,
    SA_PCM_FORMAT_S32_BE,
    SA_PCM_FORMAT_FLOAT32_LE,
    SA_PCM_FORMAT_FLOAT32_BE,
    _SA_PCM_FORMAT_MAX
} sa_pcm_format_t;

/* Native/reverse endianness definitions for PCM */
#ifdef SA_LITTLE_ENDIAN
#define SA_PCM_FORMAT_S16_NE SA_PCM_FORMAT_S16_LE
#define SA_PCM_FORMAT_S24_NE SA_PCM_FORMAT_S24_LE
#define SA_PCM_FORMAT_S32_NE SA_PCM_FORMAT_S32_LE
#define SA_PCM_FORMAT_FLOAT32_NE SA_PCM_FORMAT_FLOAT32_LE
#define SA_PCM_FORMAT_S16_RE SA_PCM_FORMAT_S16_BE
#define SA_PCM_FORMAT_S24_RE SA_PCM_FORMAT_S24_BE
#define SA_PCM_FORMAT_S32_RE SA_PCM_FORMAT_S32_BE
#define SA_PCM_FORMAT_FLOAT32_RE SA_PCM_FORMAT_FLOAT32_BE
#else
#define SA_PCM_FORMAT_S16_NE SA_PCM_FORMAT_S16_BE
#define SA_PCM_FORMAT_S24_NE SA_PCM_FORMAT_S24_BE
#define SA_PCM_FORMAT_S32_NE SA_PCM_FORMAT_S32_BE
#define SA_PCM_FORMAT_FLOAT32_NE SA_PCM_FORMAT_FLOAT32_BE
#define SA_PCM_FORMAT_S16_RE SA_PCM_FORMAT_S16_LE
#define SA_PCM_FORMAT_S24_RE SA_PCM_FORMAT_S24_LE
#define SA_PCM_FORMAT_S32_RE SA_PCM_FORMAT_S32_LE
#define SA_PCM_FORMAT_FLOAT32_RE SA_PCM_FORMAT_FLOAT32_LE
#endif

#define SA_CODEC_MPEG "mpeg"
#define SA_CODEC_AC3 "ac3"
#define SA_CODEC_GSM "gsm"
#define SA_CODEC_VORBIS "vorbis"
#define SA_CODEC_SPEEX "speex"

/** Device opening modes */
typedef enum {
    SA_MODE_WRONLY = 1,
    SA_MODE_RDONLY = 2,
    SA_MODE_RDWR   = 3,
    _SA_MODE_MAX   = 4
} sa_mode_t;

/** Error codes */
typedef enum {
    SA_SUCCESS = 0,
    SA_ERROR_NOT_SUPPORTED = -1,
    SA_ERROR_INVALID = -2,
    SA_ERROR_STATE = -3,
    SA_ERROR_OOM = -4,
    SA_ERROR_NO_DEVICE = -5,
    SA_ERROR_NO_DRIVER = -6,
    SA_ERROR_NO_CODEC = -7,
    SA_ERROR_NO_PCM_FORMAT = -7,
    SA_ERROR_SYSTEM = -8,
    SA_ERROR_NO_INIT = -9,
    SA_ERROR_NO_META = -10,
    SA_ERROR_NO_DATA = -11,
    SA_ERROR_NO_SPACE = -12,
    _SA_ERROR_MAX = -13
} sa_error_t;

/** Possible events for notifications */
typedef enum {
    SA_NOTIFY_REQUEST_START,
    SA_NOTIFY_REQUEST_STOP,
    SA_NOTIFY_CHANGED_READ_VOLUME,
    SA_NOTIFY_CHANGED_WRITE_VOLUME,
    SA_NOTIFY_CHANGED_DEVICE,
    _SA_NOTIFY_MAX
} sa_notify_t;

/** Classes of events */
typedef enum {
    SA_EVENT_REQUEST_IO,
    SA_EVENT_INIT_THREAD,
    SA_EVENT_NOTIFY,
    SA_EVENT_ERROR,
    _SA_EVENT_MAX
} sa_event_t;

/** List of sample position queries */
typedef enum {
    SA_POSITION_WRITE_DELAY,
    SA_POSITION_WRITE_HARDWARE,
    SA_POSITION_WRITE_SOFTWARE,
    SA_POSITION_READ_DELAY,
    SA_POSITION_READ_HARDWARE,
    SA_POSITION_READ_SOFTWARE,
    SA_POSITION_DUPLEX_DELAY,
    _SA_POSITION_MAX
} sa_position_t;

/* Channel positions */
typedef enum {
    SA_CHANNEL_MONO,
    SA_CHANNEL_LEFT,
    SA_CHANNEL_RIGHT,
    SA_CHANNEL_CENTER,
    SA_CHANNEL_FRONT_LEFT,
    SA_CHANNEL_FRONT_RIGHT,
    SA_CHANNEL_FRONT_CENTER,
    SA_CHANNEL_REAR_LEFT,
    SA_CHANNEL_REAR_RIGHT,
    SA_CHANNEL_REAR_CENTER,
    SA_CHANNEL_LFE,
    SA_CHANNEL_FRONT_LEFT_OF_CENTER,
    SA_CHANNEL_FRONT_RIGHT_OF_CENTER,
    SA_CHANNEL_SIDE_LEFT,
    SA_CHANNEL_SIDE_RIGHT,
    SA_CHANNEL_TOP_CENTER,
    SA_CHANNEL_TOP_FRONT_LEFT,
    SA_CHANNEL_TOP_FRONT_RIGHT,
    SA_CHANNEL_TOP_FRONT_CENTER,
    SA_CHANNEL_TOP_REAR_LEFT,
    SA_CHANNEL_TOP_REAR_RIGHT,
    SA_CHANNEL_TOP_REAR_CENTER,
    SA_CHANNEL_AUX0,
    SA_CHANNEL_AUX1,
    SA_CHANNEL_AUX2,
    SA_CHANNEL_AUX3,
    SA_CHANNEL_AUX4,
    SA_CHANNEL_AUX5,
    SA_CHANNEL_AUX6,
    SA_CHANNEL_AUX7,
    SA_CHANNEL_AUX8,
    SA_CHANNEL_AUX9,
    SA_CHANNEL_AUX10,
    SA_CHANNEL_AUX11,
    SA_CHANNEL_AUX12,
    SA_CHANNEL_AUX13,
    SA_CHANNEL_AUX14,
    SA_CHANNEL_AUX15,
    SA_CHANNEL_AUX16,
    SA_CHANNEL_AUX17,
    SA_CHANNEL_AUX18,
    SA_CHANNEL_AUX19,
    SA_CHANNEL_AUX20,
    SA_CHANNEL_AUX21,
    SA_CHANNEL_AUX22,
    SA_CHANNEL_AUX23,
    SA_CHANNEL_AUX24,
    SA_CHANNEL_AUX25,
    SA_CHANNEL_AUX26,
    SA_CHANNEL_AUX27,
    SA_CHANNEL_AUX28,
    SA_CHANNEL_AUX29,
    SA_CHANNEL_AUX30,
    SA_CHANNEL_AUX31,
    _SA_CHANNEL_MAX
} sa_channel_t;

typedef enum {
    SA_STATE_INIT,
    SA_STATE_RUNNING,
    SA_STATE_STOPPED,
    /* put more stuff */
    _SA_STATE_MAX
} sa_state_t;

typedef enum {
    SA_XRUN_MODE_STOP,
    SA_XRUN_MODE_SPIN,
    _SA_XRUN_MODE_MAX
} sa_xrun_mode_t;

typedef enum {
    SA_ADJUST_UP = 1,
    SA_ADJUST_DOWN = -1,
    SA_ADJUST_NONE = 0
} sa_adjust_t;

/* Some kind of meta information.  */
#define SA_META_CLIENT_NAME "sydney.client-name"     /* utf-8 */ 
#define SA_META_PROCESS_ID "sydney.process-id"       /* getpid() */
#define SA_META_LANGUAGE "sydney.language"           /* de_DE and similar */

/* Some kind of meta information. Not filled in */
#define SA_META_STREAM_NAME "sydney.stream-name"     /* utf-8 */ 
#define SA_META_ICON_NAME "sydney.icon-name"         /* file name (no slashes) */
#define SA_META_ICON_PNG "sydney.icon-png"           /* PNG blob */
#define SA_META_ROLE "sydney.role"                   /* one of: "music", "phone", "game", "event" */
#define SA_META_X11_DISPLAY "sydney.x11-display"     /* X11 display */
#define SA_META_X11_WINDOW "sydney.x11-window"       /* X11 window id */

/** Main callback function */
typedef int (*sa_event_callback_t)(sa_stream_t *s, sa_event_t event);

/** Create an opaque (e.g. AC3) codec stream */
int sa_stream_create_opaque(sa_stream_t **s, const char *client_name, sa_mode_t mode, const char *codec);

/** Normal way to open a PCM device */
int sa_stream_create_pcm(sa_stream_t **s, const char *client_name, sa_mode_t mode, sa_pcm_format_t format, unsigned int rate, unsigned int nchannels);

/** Initialise the device */
int sa_stream_open(sa_stream_t *s);

/** Close/destroy everything */
int sa_stream_destroy(sa_stream_t *s);

/* "Soft" params */
int sa_stream_set_write_lower_watermark(sa_stream_t *s, size_t size);
int sa_stream_set_read_lower_watermark(sa_stream_t *s, size_t size);
int sa_stream_set_write_upper_watermark(sa_stream_t *s, size_t size);
int sa_stream_set_read_upper_watermark(sa_stream_t *s, size_t size);

/** Set the mapping between channels and the loudspeakers */
int sa_stream_set_channel_map(sa_stream_t *s, const sa_channel_t map[], unsigned int n);

/** Whether xruns cause the card to reset */
int sa_stream_set_xrun_mode(sa_stream_t *s, sa_xrun_mode_t mode);

/** Set the device to non-interleaved mode */
int sa_stream_set_non_interleaved(sa_stream_t *s, int enable);

/** Require dynamic sample rate */
int sa_stream_set_dynamic_rate(sa_stream_t *s, int enable);

/** Select driver */
int sa_stream_set_driver(sa_stream_t *s, const char *driver);

/** Start callback */
int sa_stream_start_thread(sa_stream_t *s, sa_event_callback_t callback);

/** Start callback */
int sa_stream_stop_thread(sa_stream_t *s);

/** Change the device connected to the stream */
int sa_stream_change_device(sa_stream_t *s, const char *device_name);

/** volume in hundreths of dB*/
int sa_stream_change_read_volume(sa_stream_t *s, const int32_t vol[], unsigned int n);

/** volume in hundreths of dB*/
int sa_stream_change_write_volume(sa_stream_t *s, const int32_t vol[], unsigned int n);

/** Change the sampling rate */
int sa_stream_change_rate(sa_stream_t *s, unsigned int rate);

/** Change some meta data that is attached to the stream */
int sa_stream_change_meta_data(sa_stream_t *s, const char *name, const void *data, size_t size);

/** Associate opaque user data */
int sa_stream_change_user_data(sa_stream_t *s, const void *value);

/* Hardware-related. This is implementation-specific and hardware specific. */
int sa_stream_set_adjust_rate(sa_stream_t *s, sa_adjust_t direction);
int sa_stream_set_adjust_nchannels(sa_stream_t *s, sa_adjust_t direction);
int sa_stream_set_adjust_pcm_format(sa_stream_t *s, sa_adjust_t direction);
int sa_stream_set_adjust_watermarks(sa_stream_t *s, sa_adjust_t direction);

/* Query functions */

int sa_stream_get_mode(sa_stream_t *s, sa_mode_t *access_mode);
int sa_stream_get_codec(sa_stream_t *s, char *codec, size_t *size);
int sa_stream_get_pcm_format(sa_stream_t *s, sa_pcm_format_t *format);
int sa_stream_get_rate(sa_stream_t *s, unsigned int *rate);
int sa_stream_get_nchannels(sa_stream_t *s, int *nchannels);
int sa_stream_get_user_data(sa_stream_t *s, void **value);
int sa_stream_get_write_lower_watermark(sa_stream_t *s, size_t *size);
int sa_stream_get_read_lower_watermark(sa_stream_t *s, size_t *size);
int sa_stream_get_write_upper_watermark(sa_stream_t *s, size_t *size);
int sa_stream_get_read_upper_watermark(sa_stream_t *s, size_t *size);
int sa_stream_get_channel_map(sa_stream_t *s, sa_channel_t map[], unsigned int *n);
int sa_stream_get_xrun_mode(sa_stream_t *s, sa_xrun_mode_t *mode);
int sa_stream_get_non_interleaved(sa_stream_t *s, int *enabled);
int sa_stream_get_dynamic_rate(sa_stream_t *s, int *enabled);
int sa_stream_get_driver(sa_stream_t *s, char *driver_name, size_t *size);
int sa_stream_get_device(sa_stream_t *s, char *device_name, size_t *size);
int sa_stream_get_read_volume(sa_stream_t *s, int32_t vol[], unsigned int *n);
int sa_stream_get_write_volume(sa_stream_t *s, int32_t vol[], unsigned int *n);
int sa_stream_get_meta_data(sa_stream_t *s, const char *name, void*data, size_t *size);
int sa_stream_get_adjust_rate(sa_stream_t *s, sa_adjust_t *direction);
int sa_stream_get_adjust_nchannels(sa_stream_t *s, sa_adjust_t *direction);
int sa_stream_get_adjust_pcm_format(sa_stream_t *s, sa_adjust_t *direction);
int sa_stream_get_adjust_watermarks(sa_stream_t *s, sa_adjust_t *direction);

/** Get current state of the audio device */
int sa_stream_get_state(sa_stream_t *s, sa_state_t *state);

/** Obtain the error code */
int sa_stream_get_event_error(sa_stream_t *s, sa_error_t *error);

/** Obtain the notification code */
int sa_stream_get_event_notify(sa_stream_t *s, sa_notify_t *notify);

/** sync/timing */
int sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos);


/* Blocking IO calls */

/** Interleaved capture function */
int sa_stream_read(sa_stream_t *s, void *data, size_t nbytes);
/** Non-interleaved capture function */
int sa_stream_read_ni(sa_stream_t *s, unsigned int channel, void *data, size_t nbytes);

/** Interleaved playback function */
int sa_stream_write(sa_stream_t *s, const void *data, size_t nbytes);
/** Non-interleaved playback function */
int sa_stream_write_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes);
/** Interleaved playback function with seek offset */
int sa_stream_pwrite(sa_stream_t *s, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence);
/** Non-interleaved playback function with seek offset */
int sa_stream_pwrite_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence);


/** Query how much can be read without blocking */
int sa_stream_get_read_size(sa_stream_t *s, size_t *size);
/** Query how much can be written without blocking */
int sa_stream_get_write_size(sa_stream_t *s, size_t *size);


/* Control/xrun */

/** Resume playing after a pause */
int sa_stream_resume(sa_stream_t *s);

/** Pause audio playback (do not empty the buffer) */
int sa_stream_pause(sa_stream_t *s);

/** Block until all audio has been played */
int sa_stream_drain(sa_stream_t *s);

/** Return a human readable error */
const char *sa_strerror(int code);

/* Extensions */
int
sa_stream_set_volume_abs(sa_stream_t *s, float vol);

int
sa_stream_get_volume_abs(sa_stream_t *s, float *vol);

#endif
