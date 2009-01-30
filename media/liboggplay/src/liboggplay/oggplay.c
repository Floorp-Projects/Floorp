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

  OggPlay * me = (OggPlay *)malloc(sizeof(OggPlay));

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

  return me;

}

OggPlayErrorCode
oggplay_initialise(OggPlay *me, int block) {

  OggPlayErrorCode  return_val;
  int               i;

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
  oggz_io_set_read(me->oggz, me->reader->io_read, me->reader);
  oggz_io_set_seek(me->oggz, me->reader->io_seek, me->reader);
  oggz_io_set_tell(me->oggz, me->reader->io_tell, me->reader);
  oggz_set_read_callback(me->oggz, -1, oggplay_callback_predetected, me);

  while (1) {

    if (oggz_read(me->oggz, OGGZ_READ_CHUNK_SIZE) <= 0) {
      return E_OGGPLAY_BAD_INPUT;
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

  OggPlay *me = oggplay_new_with_reader(reader);

  int r = E_OGGPLAY_TIMEOUT;
  while (r == E_OGGPLAY_TIMEOUT) {
    r = oggplay_initialise(me, 0);
  }

  if (r != E_OGGPLAY_OK) {
    free(me);
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
oggplay_set_offset(OggPlay *me, int track, ogg_int64_t offset) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track <= 0 || track > me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  me->decode_data[track]->offset = (offset << 32);

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

int
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

int
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

int
oggplay_get_kate_category(OggPlay *me, int track, const char** category) {

  OggPlayKateDecode * decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_KATE) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayKateDecode *)(me->decode_data[track]);

#ifdef HAVE_KATE
  (*category) = decode->k.ki->category;
  return E_OGGPLAY_OK;
#else
  return E_OGGPLAY_NO_KATE_SUPPORT;
#endif
}

int
oggplay_get_kate_language(OggPlay *me, int track, const char** language) {

  OggPlayKateDecode * decode;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track < 0 || track >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track]->decoded_type != OGGPLAY_KATE) {
    return E_OGGPLAY_WRONG_TRACK_TYPE;
  }

  decode = (OggPlayKateDecode *)(me->decode_data[track]);

#ifdef HAVE_KATE
  (*language) = decode->k.ki->language;
  return E_OGGPLAY_OK;
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
   * clean up any trash pointers.  As soon as the current buffer has a
   * frame taken out, we know the old buffer will no longer be used.
   */

  if (me->trash != NULL && me->buffer->last_emptied > -1) {
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

    /* end-of-file */
    if (r == 0) {
      num_records = oggplay_callback_info_prepare(me, &info);
     /*
       * set all of the tracks to active
       */
      for (i = 0; i < me->num_tracks; i++) {
        me->decode_data[i]->active = 0;
        me->active_tracks = 0;
      }

      if (info != NULL) {
        me->callback (me, num_records, info, me->callback_user_ptr);
        oggplay_callback_info_destroy(me, info);
      }

      /*
       * ensure all tracks have their final data packet set to end_of_stream.
       * But skip doing this if we're shutting down --- me->buffer may not
       * be in a safe state.
       */
      if (me->buffer != NULL && !me->shutdown) {
        oggplay_buffer_set_last_data(me, me->buffer);
      }

      return E_OGGPLAY_OK;
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

  if (info == NULL) {
    goto read_more_data;
  }

  me->target += me->callback_period;
  if (me->shutdown) {
    return E_OGGPLAY_OK;
  }
  if (r == -1) {
    return E_OGGPLAY_USER_INTERRUPT;
  }

  return E_OGGPLAY_CONTINUE;

}

OggPlayErrorCode
oggplay_start_decoding(OggPlay *me) {

  int r;

  while (1) {
    if ((r = oggplay_step_decoding(me)) != E_OGGPLAY_CONTINUE)
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

  for (i = 0; i < me->num_tracks; i++) {
    oggplay_callback_shutdown(me->decode_data[i]);
  }

  oggz_close(me->oggz);

  if (me->buffer != NULL) {
    oggplay_buffer_shutdown(me, me->buffer);
  }

  free(me);

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

  if (me->reader->duration) 
    return me->reader->duration(me->reader);
  else {
    ogg_int64_t pos;
    ogg_int64_t duration;
    pos = oggz_tell_units(me->oggz);
    duration = oggz_seek_units(me->oggz, 0, SEEK_END);
    oggz_seek_units(me->oggz, pos, SEEK_SET);
    oggplay_seek_cleanup(me, pos);
    return duration;
  }
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
