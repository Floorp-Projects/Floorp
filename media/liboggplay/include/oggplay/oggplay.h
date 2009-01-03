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
 * oggplay.h
 * 
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */

#ifndef __OGGPLAY_H__
#define __OGGPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <oggplay/oggplay_enums.h>
#include <oggplay/oggplay_reader.h>

typedef struct _OggPlay OggPlay;
typedef struct _OggPlayCallbackInfo OggPlayCallbackInfo;
typedef int (OggPlayDataCallback)(OggPlay *player, int num_records,
                OggPlayCallbackInfo **records, void *user);

#include <oggplay/oggplay_query.h>
#include <oggplay/oggplay_callback_info.h>
#include <oggplay/oggplay_tools.h>
#include <oggplay/oggplay_seek.h>
/*
#include <oggplay/oggplay_retrieve.h>
#include <oggplay/oggplay_cmml.h>
*/

OggPlay *
oggplay_init(void);

OggPlayErrorCode
oggplay_set_reader(OggPlay *OS, OggPlayReader *OSR);

OggPlay *
oggplay_open_with_reader(OggPlayReader *reader);

OggPlay *
oggplay_new_with_reader(OggPlayReader *reader);

OggPlayErrorCode
oggplay_initialise(OggPlay *me, int block);

OggPlayErrorCode
oggplay_set_source(OggPlay *OS, char *source);

OggPlayErrorCode
oggplay_set_data_callback(OggPlay *me, OggPlayDataCallback callback, 
                            void *user);

OggPlayErrorCode
oggplay_set_callback_num_frames(OggPlay *me, int stream, int frames);

OggPlayErrorCode
oggplay_set_offset(OggPlay *me, int track, ogg_int64_t offset);

OggPlayErrorCode
oggplay_get_video_y_size(OggPlay *me, int track, int *y_width, int *y_height);

OggPlayErrorCode
oggplay_get_video_uv_size(OggPlay *me, int track, int *uv_width, int *uv_height);

OggPlayErrorCode
oggplay_get_audio_channels(OggPlay *me, int track, int *channels);

OggPlayErrorCode
oggplay_get_audio_samplerate(OggPlay *me, int track, int *samplerate); 

OggPlayErrorCode
oggplay_get_video_fps(OggPlay *me, int track, int* fps_denom, int* fps_num);

OggPlayErrorCode
oggplay_get_kate_category(OggPlay *me, int track, const char** category);

OggPlayErrorCode
oggplay_get_kate_language(OggPlay *me, int track, const char** language);

OggPlayErrorCode
oggplay_start_decoding(OggPlay *me);

OggPlayErrorCode
oggplay_step_decoding(OggPlay *me);

OggPlayErrorCode
oggplay_use_buffer(OggPlay *player, int size);

OggPlayCallbackInfo **
oggplay_buffer_retrieve_next(OggPlay *player);

OggPlayErrorCode
oggplay_buffer_release(OggPlay *player, OggPlayCallbackInfo **track_info);

void
oggplay_prepare_for_close(OggPlay *me);

OggPlayErrorCode
oggplay_close(OggPlay *player);

int
oggplay_get_available(OggPlay *player);

ogg_int64_t
oggplay_get_duration(OggPlay * player);

int
oggplay_media_finished_retrieving(OggPlay * player);

#ifdef __cplusplus
}
#endif

#endif
