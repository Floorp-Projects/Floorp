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
 * oggplay.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */

#include "oggplay_private.h"
#include "oggplay_buffer.h"

#include <string.h>
#include <stdlib.h>

#define OGGZ_READ_CHUNK_SIZE 8192

OggPlay *
oggplay_new_with_reader(OggPlayReader *reader) {

  OggPlay * me = NULL;

  /* check whether the reader is valid. */
  if (reader == NULL)
    return NULL;

  me = (OggPlay *)oggplay_malloc (sizeof(OggPlay));
  if (me == NULL)
	  return NULL;

  me->reader = reader;
  me->decode_data = NULL;
  me->callback_info = NULL;
  me->num_tracks = 0;
  me->all_tracks_initialised = 0;
  me->callback_period = 0;
  me->callback = NULL;
  me->target = 0L;
  me->active_tracks = 0;
  me->buffer = NULL;
  me->shutdown = 0;
  me->trash = NULL;
  me->oggz = NULL;
  me->pt_update_valid = 1;
  me->duration = -1;

  return me;

}

OggPlayErrorCode
oggplay_initialise(OggPlay *me, int block) {

  OggPlayErrorCode  return_val;
  int               i;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }
  
  return_val = me->reader->initialise(me->reader, block);

  if (return_val != E_OGGPLAY_OK) {
    return return_val;
  }

  /*
   * this is the cut-off time value below which packets will be ignored.  Initialise it to 0 here.
   * We'll reinitialise it when/if we encounter a skeleton header
   */
  me->presentation_time = 0;

  /*
   * start to retrieve data, until we get all of the track info.  We need
   * to do this now so that the user can query us for this info before entering
   * the main loop
   */
  me->oggz = oggz_new(OGGZ_READ | OGGZ_AUTO);
  if (me->oggz == NULL)
    return E_OGGPLAY_OGGZ_UNHAPPY;

  if (oggz_io_set_read(me->oggz, me->reader->io_read, me->reader) != 0)
    return E_OGGPLAY_OGGZ_UNHAPPY;

  if (oggz_io_set_seek(me->oggz, me->reader->io_seek, me->reader) != 0)
    return E_OGGPLAY_OGGZ_UNHAPPY;

  if (oggz_io_set_tell(me->oggz, me->reader->io_tell, me->reader) != 0)
    return E_OGGPLAY_OGGZ_UNHAPPY;

  if (oggz_set_read_callback(me->oggz, -1, oggplay_callback_predetected, me))
    return E_OGGPLAY_OGGZ_UNHAPPY;

  while (1) {
    i = oggz_read (me->oggz, OGGZ_READ_CHUNK_SIZE);
    
    switch (i) {
      case 0:
        /* 
         * EOF reached while processing headers,
         * possible erroneous file, mark it as such.
         */
      case OGGZ_ERR_HOLE_IN_DATA:
        /* there was a whole in the data */
        return E_OGGPLAY_BAD_INPUT;
      
      case OGGZ_ERR_OUT_OF_MEMORY:
        /* ran out of memory during decoding! */
        return E_OGGPLAY_OUT_OF_MEMORY;
      
      case OGGZ_ERR_STOP_ERR:
        /* */
        return E_OGGPLAY_BAD_OGGPLAY;
    }

    if (me->all_tracks_initialised) {
      break;
    }
  }

  /*
   * set all the tracks to inactive
   */
  for (i = 0; i < me->num_tracks; i++) {
    me->decode_data[i]->active = 0;
  }

  /*
   * if the buffer was set up before initialisation, prepare it now
   */
  if (me->buffer != NULL) {
    oggplay_buffer_prepare(me);
  }

  return E_OGGPLAY_OK;

}

OggPlay *
oggplay_open_with_reader(OggPlayReader *reader) {

  OggPlay *me = NULL;
  int r = E_OGGPLAY_TIMEOUT;

  if ((me = oggplay_new_with_reader(reader)) == NULL)
    return NULL;

  while (r == E_OGGPLAY_TIMEOUT) {
    r = oggplay_initialise(me, 0);
  }

  if (r != E_OGGPLAY_OK) {
    
    /* in case of error close the OggPlay handle */
    oggplay_close(me);

    return NULL;
  }

  return me;
}

/*
 * API function to prevent bad input, and to prevent data callbacks being registered
 * in buffer mode
 */
OggPlayErrorCode
oggplay_set_data_callback(OggPlay *me, OggPlayDataCallback callback,
                          void *user) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->buffer != NULL) {
    return E_OGGPLAY_BUFFER_MODE;
  }

  oggplay_set_data_callback_force(me, callback, user);
  return E_OGGPLAY_OK;

}

/*
 * internal function that doesn't perform error checking.  Used so the buffer
 * can register a callback!
 */
void
oggplay_set_data_callback_force(OggPlay *me, OggPlayDataCallback callback,
                                void *user) {

  me->callback = callback;
  me->callback_user_ptr = user;

}


OggPlayErrorCode
oggplay_set_callback_num_frames(OggPlay *me, int track, int frames) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  me->callback_period = me->decode_data[track]->granuleperiod * frames;
  me->target = me->presentation_time + me->callback_period - 1;

  return E_OGGPLAY_OK;
}

OggPlayErrorCode
oggplay_set_callback_period(OggPlay *me, int track, int milliseconds) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  me->callback_period = OGGPLAY_TIME_INT_TO_FP(((ogg_int64_t)milliseconds))/1000;
  me->target = me->presentation_time + me->callback_period - 1;

  return E_OGGPLAY_OK;
}

OggPlayErrorCode
oggplay_set_offset(OggPlay *me, int track, ogg_int64_t offset) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  me->decode_data[track]->offset = OGGPLAY_TIME_INT_TO_FP(offset) / 1000;

  return E_OGGPLAY_OK;

}

OggPlayErrorCode
oggplay_get_video_fps(OggPlay *me, int track, int* fps_denom, int* fps_num) {
  OggPlayTheoraDecode *decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_YUV_VIDEO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayTheoraDecode *)(me->decode_data[track]);

  if ((decode->video_info.fps_denominator == 0)
    || (decode->video_info.fps_numerator == 0)) {
    return E_OGGPLAY_UNINITIALISED;
  }

  (*fps_denom) = decode->video_info.fps_denominator;
  (*fps_num) = decode->video_info.fps_numerator;

  return E_OGGPLAY_OK;
}

OggPlayErrorCode
oggplay_get_video_aspect_ratio(OggPlay *me, int track, int* aspect_denom, int* aspect_num) {
  OggPlayTheoraDecode *decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_YUV_VIDEO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayTheoraDecode *)(me->decode_data[track]);

  if ((decode->video_info.aspect_denominator == 0)
    || (decode->video_info.aspect_numerator == 0)) {
    return E_OGGPLAY_UNINITIALISED;
  }

  (*aspect_denom) = decode->video_info.aspect_denominator;
  (*aspect_num) = decode->video_info.aspect_numerator;

  return E_OGGPLAY_OK;
}

OggPlayErrorCode
oggplay_convert_video_to_rgb(OggPlay *me, int track, int convert, int swap_rgb) {
  OggPlayTheoraDecode *decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->content_type != OGGZ_CONTENT_THEORA) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayTheoraDecode *)(me->decode_data[track]);

  if (decode->convert_to_rgb != convert || decode->swap_rgb != swap_rgb) {
    decode->convert_to_rgb = convert;
    decode->swap_rgb = swap_rgb;
    me->decode_data[track]->decoded_type = convert ? OGGPLAY_RGBA_VIDEO : OGGPLAY_YUV_VIDEO;

    /* flush any records created with previous type */
    oggplay_data_free_list(me->decode_data[track]->data_list);
    me->decode_data[track]->data_list = NULL;
  }

  return E_OGGPLAY_OK;
}

OggPlayErrorCode
oggplay_get_video_y_size(OggPlay *me, int track, int *y_width, int *y_height) {

  OggPlayTheoraDecode *decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_YUV_VIDEO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayTheoraDecode *)(me->decode_data[track]);

  if (decode->y_width == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }

  (*y_width) = decode->y_width;
  (*y_height) = decode->y_height;

  return E_OGGPLAY_OK;

}

OggPlayErrorCode
oggplay_get_video_uv_size(OggPlay *me, int track, int *uv_width, int *uv_height)
{

  OggPlayTheoraDecode *decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_YUV_VIDEO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayTheoraDecode *)(me->decode_data[track]);

  if (decode->y_width == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }

  (*uv_width) = decode->uv_width;
  (*uv_height) = decode->uv_height;

  return E_OGGPLAY_OK;

}

OggPlayErrorCode
oggplay_get_audio_channels(OggPlay *me, int track, int* channels) {

  OggPlayAudioDecode *decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_FLOATS_AUDIO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayAudioDecode *)(me->decode_data[track]);

  if (decode->sound_info.channels == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }
  (*channels) = decode->sound_info.channels;
  return E_OGGPLAY_OK;

}

OggPlayErrorCode
oggplay_get_audio_samplerate(OggPlay *me, int track, int* rate) {

  OggPlayAudioDecode * decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_FLOATS_AUDIO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayAudioDecode *)(me->decode_data[track]);

  if (decode->sound_info.channels == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }
  (*rate) = decode->sound_info.samplerate;
  return E_OGGPLAY_OK;

}

OggPlayErrorCode
oggplay_get_kate_category(OggPlay *me, int track, const char** category) {

  OggPlayKateDecode * decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->content_type != OGGZ_CONTENT_KATE) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayKateDecode *)(me->decode_data[track]);

#ifdef HAVE_KATE
  if (decode->decoder.initialised) {
    (*category) = decode->k_state.ki->category;
    return E_OGGPLAY_OK;
  }
  else return E_OGGPLAY_UNINITIALISED;
#else
  return E_OGGPLAY_NO_KATE_SUPPORT;
#endif
}

OggPlayErrorCode
oggplay_get_kate_language(OggPlay *me, int track, const char** language) {

  OggPlayKateDecode * decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->content_type != OGGZ_CONTENT_KATE) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayKateDecode *)(me->decode_data[track]);

#ifdef HAVE_KATE
  if (decode->decoder.initialised) {
    (*language) = decode->k_state.ki->language;
    return E_OGGPLAY_OK;
  }
  else return E_OGGPLAY_UNINITIALISED;
#else
  return E_OGGPLAY_NO_KATE_SUPPORT;
#endif
}

OggPlayErrorCode
oggplay_set_kate_tiger_rendering(OggPlay *me, int track, int use_tiger, int swap_rgb, int default_width, int default_height) {

  OggPlayKateDecode * decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->content_type != OGGZ_CONTENT_KATE) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayKateDecode *)(me->decode_data[track]);

#ifdef HAVE_KATE
#ifdef HAVE_TIGER
  if (decode->decoder.initialised && decode->tr) {
    decode->use_tiger = use_tiger;
    decode->swap_rgb = swap_rgb;
    decode->default_width = default_width;
    decode->default_height = default_height;
    decode->decoder.decoded_type = use_tiger ? OGGPLAY_RGBA_VIDEO : OGGPLAY_KATE;
    return E_OGGPLAY_OK;
  }
  else return E_OGGPLAY_UNINITIALISED;
#else
  return E_OGGPLAY_NO_TIGER_SUPPORT;
#endif
#else
  return E_OGGPLAY_NO_KATE_SUPPORT;
#endif
}

OggPlayErrorCode
oggplay_overlay_kate_track_on_video(OggPlay *me, int kate_track, int video_track) {

  OggPlayKateDecode * decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (kate_track < 0 || kate_track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }
  if (video_track < 0 || video_track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[kate_track]->content_type != OGGZ_CONTENT_KATE) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  if (me->decode_data[kate_track]->decoded_type != OGGPLAY_RGBA_VIDEO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  if (me->decode_data[video_track]->content_type != OGGZ_CONTENT_THEORA) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  if (me->decode_data[video_track]->decoded_type != OGGPLAY_RGBA_VIDEO) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayKateDecode *)(me->decode_data[kate_track]);

#ifdef HAVE_KATE
#ifdef HAVE_TIGER
  decode->overlay_dest = video_track;
  return E_OGGPLAY_OK;
#else
  return E_OGGPLAY_NO_TIGER_SUPPORT;
#endif
#else
  return E_OGGPLAY_NO_KATE_SUPPORT;
#endif
}

#define MAX_CHUNK_COUNT   10

OggPlayErrorCode
oggplay_step_decoding(OggPlay *me) {

  OggPlayCallbackInfo  ** info;
  int                     num_records;
  int                     r;
  int                     i;
  int                     need_data  = 0;
  int                     chunk_count = 0;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }
  
  /* 
   * check whether the OggPlayDataCallback is set for the given
   * OggPlay handle. If not return with error as there's no callback 
   * function processing the decoded data.
   */
  if (me->callback == NULL) {
    return E_OGGPLAY_UNINITIALISED;
  }

  /*
   * clean up any trash pointers.  As soon as the current buffer has a
   * frame taken out, we know the old buffer will no longer be used.
   */

  if 
  (
    me->trash != NULL 
    && 
    (me->buffer == NULL || me->buffer->last_emptied > -1)
  ) 
  {
    oggplay_take_out_trash(me, me->trash);
    me->trash = NULL;
  }

read_more_data:

  while (1) {
     /*
     * if there are no active tracks, we might need to return some data
     * left over at the end of a once-active track that has had all of its
     * data processed.  Look through the tracks to find these overhangs
     */
    int r;

    if (me->active_tracks == 0) {
      int remaining = 0;
      for (i = 0; i < me->num_tracks; i++) {
        if (me->decode_data[i]->current_loc +
                     me->decode_data[i]->granuleperiod >= me->target + me->decode_data[i]->offset) {
          remaining++;
        }
      }
      if (remaining == 0) {
        return E_OGGPLAY_OK;
      }
    }

    /*
     * if any of the tracks have not yet met the target (modified by that
     * track's offset), then retrieve more data
     */
    need_data = 0;
    for (i = 0; i < me->num_tracks; i++) {
      if (me->decode_data[i]->active == 0)
        continue;
      if (me->decode_data[i]->content_type == OGGZ_CONTENT_CMML)
        continue;
      if (me->decode_data[i]->content_type == OGGZ_CONTENT_KATE)
        continue;
      if
      (
        me->decode_data[i]->current_loc
        <
        me->target + me->decode_data[i]->offset
      )
      {
        need_data = 1;
        break;
      }
    }

    if (!need_data) {
      break;
    }

    /*
     * get a chunk of data.  If we're at the end of the file, then we must
     * have some final frames to render (?).  E_OGGPLAY_END_OF_FILE is
     * only returned if there is *no* more data.
     */

    if (chunk_count > MAX_CHUNK_COUNT) {
      return E_OGGPLAY_TIMEOUT;
    }

    chunk_count += 1;

    r = oggz_read(me->oggz, OGGZ_READ_CHUNK_SIZE);

    switch (r) {
      case 0:
        /* end-of-file */
        
        num_records = oggplay_callback_info_prepare(me, &info);
        /*
        * set all of the tracks to inactive
        */
        for (i = 0; i < me->num_tracks; i++) {
          me->decode_data[i]->active = 0;
        }
        me->active_tracks = 0;

        if (info != NULL) {
          me->callback (me, num_records, info, me->callback_user_ptr);
          oggplay_callback_info_destroy(me, info);
        }

        /*
        * ensure all tracks have their final data packet set to end_of_stream
        * But skip doing this if we're shutting down --- me->buffer may not
        * be in a safe state.
        */
        if (me->buffer != NULL && !me->shutdown) {
          oggplay_buffer_set_last_data(me, me->buffer);
        }

        /* we reached the end of the stream */
        return E_OGGPLAY_OK;
              
      case OGGZ_ERR_HOLE_IN_DATA:
        /* there was a whole in the data */
        return E_OGGPLAY_BAD_INPUT;

      case OGGZ_ERR_STOP_ERR:
        /* 
         * one of the callback functions requested us to stop.
         * as this currently happens only when one of the 
         * OggzReadPacket callback functions does not receive
         * the user provided data, i.e. the OggPlayDecode struct
         * for the track mark it as a memory problem, since this
         * could happen only if something is wrong with the memory,
         * e.g. some buffer overflow.
         */
      
      case OGGZ_ERR_OUT_OF_MEMORY:
        /* ran out of memory during decoding! */
        return E_OGGPLAY_OUT_OF_MEMORY;
                
      default:
        break;
    }
  }
  /*
   * prepare a callback
   */
  num_records = oggplay_callback_info_prepare (me, &info);
  if (info != NULL) {
    r = me->callback (me, num_records, info, me->callback_user_ptr);
    oggplay_callback_info_destroy (me, info);
  } else {
    r = 0;
  }

  /*
   * clean the data lists
   */
  for (i = 0; i < me->num_tracks; i++) {
    oggplay_data_clean_list (me->decode_data[i]);
  }
  
  /* 
   * there was an error during info prepare!
   * abort decoding! 
   */
  if (num_records < 0) {
    return num_records;
  }
  
  /* if we received an shutdown event, dont try to read more data...*/
  if (me->shutdown) {
    return E_OGGPLAY_OK;
  }
  
  /* we require more data for decoding */
  if (info == NULL) {
    goto read_more_data;
  }

  me->target += me->callback_period;
  if (r == -1) {
    return E_OGGPLAY_USER_INTERRUPT;
  }

  return E_OGGPLAY_CONTINUE;

}

OggPlayErrorCode
oggplay_start_decoding(OggPlay *me) {

  int r;

  while (1) {
    r = oggplay_step_decoding(me);
    if (r == E_OGGPLAY_CONTINUE || r == E_OGGPLAY_TIMEOUT) {
      continue;
    }
    return (OggPlayErrorCode)r;
  }
}

OggPlayErrorCode
oggplay_close(OggPlay *me) {

  int i;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->reader != NULL) {
    me->reader->destroy(me->reader);
  }

  /* */
  if (me->decode_data != NULL) {
    for (i = 0; i < me->num_tracks; i++) {
      oggplay_callback_shutdown(me->decode_data[i]);
    }
  }

  if (me->oggz)
    oggz_close(me->oggz);

  if (me->buffer != NULL) {
    oggplay_buffer_shutdown(me, me->buffer);
  }

  if (me->callback_info != NULL) {
    oggplay_free(me->callback_info);
  }
    
  oggplay_free(me->decode_data);
  oggplay_free(me);

  return E_OGGPLAY_OK;
}

/*
 * this function is required to release the frame_sem in the buffer, if
 * the buffer is being used.
 */
void
oggplay_prepare_for_close(OggPlay *me) {

  me->shutdown = 1;
  if (me->buffer != NULL) {
    SEM_SIGNAL(((OggPlayBuffer *)(me->buffer))->frame_sem);
  }
}

int
oggplay_get_available(OggPlay *me) {

  ogg_int64_t current_time, current_byte;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  current_time = oggz_tell_units(me->oggz);
  current_byte = (ogg_int64_t)oggz_tell(me->oggz);

  return me->reader->available(me->reader, current_byte, current_time);

}

ogg_int64_t
oggplay_get_duration(OggPlay *me) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  /* If the reader has a duration function we always call that
   * function to find the duration. We never cache the result
   * of that function.
   *
   * If there is no reader duration function we use our cached
   * duration value, or do a liboggz seek to find it and cache
   * that.
   */
  if (me->reader->duration) {
      ogg_int64_t d = me->reader->duration(me->reader);
      if (d >= 0) {
        me->duration = d;
      }
  }

  if (me->duration < 0) {
    ogg_int64_t pos;
    pos = oggz_tell_units(me->oggz);
    me->duration = oggz_seek_units(me->oggz, 0, SEEK_END);
    oggz_seek_units(me->oggz, pos, SEEK_SET);
    oggplay_seek_cleanup(me, pos);
  }

  return me->duration;
}

int
oggplay_media_finished_retrieving(OggPlay *me) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->reader == NULL) {
    return E_OGGPLAY_BAD_READER;
  }

  return me->reader->finished_retrieving(me->reader);

}

