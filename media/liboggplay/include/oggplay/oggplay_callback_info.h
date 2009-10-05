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

/** @file
 * oggplay_callback_info.h
 * 
 * @authors
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 * Viktor Gal
 */

#ifndef __OGGPLAY_CALLBACK_INFO__
#define __OGGPLAY_CALLBACK_INFO__

/** structure for storing a YUV video frame */
typedef struct {
  unsigned char   * y; /**< Y-plane */
  unsigned char   * u; /**< U-plane*/
  unsigned char   * v; /**< V-plane */
} OggPlayVideoData;

/** structure for storing a video frame in RGB fromat */
typedef struct {
  unsigned char   * rgba; /**< may be NULL if no alpha */
  unsigned char   * rgb;  /**< may be NULL if alpha */
  size_t          width;  /**< width in pixels */
  size_t          height; /**< height in pixels */
  size_t          stride; /**< stride */
} OggPlayOverlayData;

/** Type for representing audio data */
typedef void * OggPlayAudioData;

/** Type for representing text data */
typedef char OggPlayTextData;

struct _OggPlayDataHeader;
/** Header for the various data formats */
typedef struct _OggPlayDataHeader OggPlayDataHeader;

/**
 * Get the data type of the given OggPlayCallbackInfo.
 * 
 * @param info 
 * @returns The data type of the given OggPlayCallbackInfo
 * @see OggPlayDataType
 */
OggPlayDataType
oggplay_callback_info_get_type(OggPlayCallbackInfo *info);

int
oggplay_callback_info_get_available(OggPlayCallbackInfo *info);


int
oggplay_callback_info_get_required(OggPlayCallbackInfo *info);

/**
 * Get the array of records stored in the OggPlayCallbackInfo
 * 
 * @param info
 * @returns array of records 
 * @retval NULL if the supplied OggPlayCallbackInfo is a NULL pointer
 */
OggPlayDataHeader **
oggplay_callback_info_get_headers(OggPlayCallbackInfo *info);

/**
 * Get the size of the given record.
 * 
 * @param header
 * @returns The number of samples in the record.
 */
ogg_int64_t
oggplay_callback_info_get_record_size(OggPlayDataHeader *header);

/**
 * Extract the video frame from the supplied record.
 *
 * @param header 
 * @returns the video frame
 * @retval NULL if the supplied OggPlayCallbackInfo is a NULL pointer
 */
OggPlayVideoData *
oggplay_callback_info_get_video_data(OggPlayDataHeader *header);

/**
 * Extract the overlay data from the supplied record.
 * 
 * @param header
 * @returns OggPlayOverlayData
 * @retval NULL if the supplied OggPlayCallbackInfo is a NULL pointer
 */
OggPlayOverlayData *
oggplay_callback_info_get_overlay_data(OggPlayDataHeader *header);

/**
 * Extract the audio data from the supplied record.
 *
 * @param header
 * @returns OggPlayAudioData.
 * @retval NULL if the supplied OggPlayCallbackInfo is a NULL pointer
 */
OggPlayAudioData *
oggplay_callback_info_get_audio_data(OggPlayDataHeader *header);

/**
 * Extract the text data from the supplied record.
 *
 * @param header
 * @returns OggPlayTextData 
 * @retval NULL if the supplied OggPlayCallbackInfo is a NULL pointer
 */
OggPlayTextData *
oggplay_callback_info_get_text_data(OggPlayDataHeader *header);

/**
 * Get the state of the stream.
 *
 * @param info
 * @returns State of the given stream.
 * @see OggPlayStreamInfo
 */
OggPlayStreamInfo
oggplay_callback_info_get_stream_info(OggPlayCallbackInfo *info);


void
oggplay_callback_info_lock_item(OggPlayDataHeader *header);

void
oggplay_callback_info_unlock_item(OggPlayDataHeader *header);

/**
 * Get the presentation time of the given record.
 *  
 * @param header
 * @returns presentation time of the given frame in milliseconds.
 */
long
oggplay_callback_info_get_presentation_time(OggPlayDataHeader *header);

#endif
