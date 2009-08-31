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
 * oggplay_callback_info.h
 * 
 * Shane Stephens <shane.stephens@annodex.net>
 */

#ifndef __OGGPLAY_CALLBACK_INFO__
#define __OGGPLAY_CALLBACK_INFO__

typedef struct {
  unsigned char   * y;
  unsigned char   * u;
  unsigned char   * v;
} OggPlayVideoData;

typedef struct {
  unsigned char   * rgba; /* may be NULL if no alpha */
  unsigned char   * rgb; /* may be NULL if alpha */
  size_t          width; /* in pixels */
  size_t          height; /* in pixels */
  size_t          stride; /* in bytes */
} OggPlayOverlayData;

typedef void * OggPlayAudioData;

typedef char OggPlayTextData;

struct _OggPlayDataHeader;
typedef struct _OggPlayDataHeader OggPlayDataHeader;

OggPlayDataType
oggplay_callback_info_get_type(OggPlayCallbackInfo *info);
int
oggplay_callback_info_get_available(OggPlayCallbackInfo *info);
int
oggplay_callback_info_get_required(OggPlayCallbackInfo *info);
OggPlayDataHeader **
oggplay_callback_info_get_headers(OggPlayCallbackInfo *info);

ogg_int64_t
oggplay_callback_info_get_record_size(OggPlayDataHeader *header);

OggPlayVideoData *
oggplay_callback_info_get_video_data(OggPlayDataHeader *header);

OggPlayOverlayData *
oggplay_callback_info_get_overlay_data(OggPlayDataHeader *header);

OggPlayAudioData *
oggplay_callback_info_get_audio_data(OggPlayDataHeader *header);

OggPlayTextData *
oggplay_callback_info_get_text_data(OggPlayDataHeader *header);

OggPlayStreamInfo
oggplay_callback_info_get_stream_info(OggPlayCallbackInfo *info);

void
oggplay_callback_info_lock_item(OggPlayDataHeader *header);

void
oggplay_callback_info_unlock_item(OggPlayDataHeader *header);

long
oggplay_callback_info_get_presentation_time(OggPlayDataHeader *header);

#endif
