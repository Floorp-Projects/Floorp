/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * oggplay_private.h
 *
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */
#ifndef __OGGPLAY_PRIVATE_H__
#define __OGGPLAY_PRIVATE_H__

#ifdef WIN32
#include "config_win32.h"
#else
#include <config.h>
#endif
#include <oggplay/oggplay.h>

#include <oggz/oggz.h>
#include <theora/theora.h>
#include <fishsound/fishsound.h>

#ifdef HAVE_KATE
#include <kate/kate.h>
#endif

#ifdef WIN32

#ifdef HAVE_WINSOCK2
#include <winsock2.h>
#else
#include <winsock.h>
#endif
#endif

// for Win32 <windows.h> has to be included last
#include "std_semaphore.h"

/**
 *
 * has_been_presented: 0 until the data has been added as a "required" element,
 *                     then 1.
 */
struct _OggPlayDataHeader {
  int                         lock;
  struct _OggPlayDataHeader * next;
  ogg_int64_t                 presentation_time;
  ogg_int64_t                 samples_in_record;
  int                         has_been_presented;
};

typedef struct {
  OggPlayDataHeader   header;
  OggPlayVideoData    data;
} OggPlayVideoRecord;

typedef struct {
  OggPlayDataHeader   header;
  void              * data;
} OggPlayAudioRecord;

typedef struct {
  OggPlayDataHeader   header;
  char              * data;
} OggPlayTextRecord;

struct _OggPlay;

typedef struct {
  void   ** buffer_list;
  void   ** buffer_mirror;
  int       buffer_size;
  int       last_filled;
  int       last_emptied;
  semaphore frame_sem;
} OggPlayBuffer;

struct _OggPlayCallbackInfo {
  OggPlayDataType       data_type;
  int                   available_records;
  int                   required_records;
  OggPlayStreamInfo     stream_info;
  OggPlayDataHeader  ** records;
  OggPlayBuffer       * buffer;
};

/**
 * OggPlayDecode
 *
 * A structure that contains information about a single track within the Ogg
 * file.
 *
 * data_list, end_of_data_list: Contain decoded data packets for this track.
 *                              These packets are time-ordered, and have a
 *                              known presentation time
 *
 * untimed_data_list:           Contains decoded data packets for this track.
 *                              These packets are reverse time-ordered, and
 *                              have not got a known presentation time.  This
 *                              list gets constructed when a new Ogg file is
 *                              first being read, and gets torn down as soon as
 *                              the first packet with a granulepos is processed
 *
 * granuleperiod                The period between adjacent samples in this
 *                              track
 */
typedef struct {
  long                  serialno;
  int                   content_type;
  const char          * content_type_name;
  OggPlayDataType       decoded_type;
  ogg_int64_t           granuleperiod;
  ogg_int64_t           last_granulepos;
  ogg_int64_t           offset;
  ogg_int64_t           current_loc;
  int                   active;
  ogg_int64_t           final_granulepos;
  struct _OggPlay     * player;
  OggPlayDataHeader   * data_list;
  OggPlayDataHeader   * end_of_data_list;
  OggPlayDataHeader   * untimed_data_list;
  OggPlayStreamInfo     stream_info;
  int                   preroll;
} OggPlayDecode;

typedef struct {
  OggPlayDecode   decoder;
  theora_state    video_handle;
  theora_info     video_info;
  theora_comment  video_comment;
  int             remaining_header_packets;
  int             granulepos_seen;
  int             frame_delta;
  int             y_width;
  int             y_height;
  int             y_stride;
  int             uv_width;
  int             uv_height;
  int             uv_stride;
  int             cached_keyframe;
} OggPlayTheoraDecode;

typedef struct {
  OggPlayDecode   decoder;
  FishSound     * sound_handle;
  FishSoundInfo   sound_info;
} OggPlayAudioDecode;

typedef struct {
  OggPlayDecode   decoder;
  int             granuleshift;
} OggPlayCmmlDecode;

typedef struct {
  OggPlayDecode   decoder;
  ogg_int64_t     presentation_time;
  ogg_int64_t     base_time;
} OggPlaySkeletonDecode;

typedef struct {
  OggPlayDecode   decoder;
#ifdef HAVE_KATE
  int             granuleshift;
  kate_state      k;
#endif
} OggPlayKateDecode;

struct OggPlaySeekTrash;

typedef struct OggPlaySeekTrash {
  OggPlayDataHeader       * old_data;
  OggPlayBuffer           * old_buffer;
  struct OggPlaySeekTrash * next;
} OggPlaySeekTrash;

struct _OggPlay {
  OggPlayReader           * reader;
  OGGZ                    * oggz;
  OggPlayDecode          ** decode_data;
  OggPlayCallbackInfo     * callback_info;
  int                       num_tracks;
  int                       all_tracks_initialised;
  ogg_int64_t               callback_period;
  OggPlayDataCallback     * callback;
  void                    * callback_user_ptr;
  ogg_int64_t               target;
  int                       active_tracks;
  volatile OggPlayBuffer  * buffer;
  ogg_int64_t               presentation_time;
  OggPlaySeekTrash        * trash;
  int                       shutdown;
  int                       pt_update_valid;
};

void
oggplay_set_data_callback_force(OggPlay *me, OggPlayDataCallback callback,
                void *user);

void
oggplay_take_out_trash(OggPlay *me, OggPlaySeekTrash *trash);

void
oggplay_seek_cleanup(OggPlay *me, ogg_int64_t milliseconds);

typedef struct {
  void (*init)(void *user_data);
  int (*callback)(OGGZ * oggz, ogg_packet * op, long serialno,
                                                          void * user_data);
  void (*shutdown)(void *user_data);
  int size;
} OggPlayCallbackFunctions;

#include "oggplay_callback.h"
#include "oggplay_data.h"
#include "oggplay_buffer.h"

#if 0
static inline void _free(void *x) {
  printf("%p\n", x);
  free(x);
}

static inline void *_malloc(int s) {
  void *x;
  printf("%d ", s);
  x = malloc(s);
  printf("%p\n", x);
  return x;
}

static inline void *_realloc(void *x, int s) {
  void *y;
  printf("%p %d ", x, s);
  y = realloc(x, s);
  printf("%p\n", y);
  return y;
}

static inline void *_calloc(int n, int s) {
  void *x;
  printf("%d %d ", n, s);
  x = calloc(n, s);
  printf("%p\n", x);
  return x;
}

#define free(x) {printf("FREE %s %d ", __FILE__, __LINE__); _free(x);}
#define malloc(s) (printf("MALLOC %s %d ", __FILE__, __LINE__), \
    _malloc(s))
#define realloc(x, s) (printf("REALLOC %s %d ", __FILE__, __LINE__), \
    _realloc(x, s))
#define calloc(n, s) (printf("CALLOC %s %d ", __FILE__, __LINE__),  \
    _calloc(n, s))
#endif

#endif
