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
 * oggplay_buffer.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 */

#include "oggplay_private.h"

#include <stdlib.h>
#include <string.h>

#define OGGPLAY_DEFAULT_BUFFER_SIZE   20
#define WRAP_INC(c, s) ((c + 1) % s)



/*
 * Call this function to initialise the oggplay lock-free buffer.  Do not use
 * the buffer and the callback together!
 */
OggPlayBuffer *
oggplay_buffer_new_buffer(int size) {

  OggPlayBuffer *buffer = NULL;
  if (size < 0) {
    size = OGGPLAY_DEFAULT_BUFFER_SIZE;
  }

  buffer = (OggPlayBuffer*)oggplay_calloc(1, sizeof (OggPlayBuffer));

  if (buffer == NULL)
    return NULL;

  buffer->buffer_list = oggplay_calloc(size, sizeof (void *));
  if (buffer->buffer_list == NULL)
    goto error;

  buffer->buffer_mirror = oggplay_calloc(size, sizeof (void *));
  if (buffer->buffer_mirror == NULL)
    goto error;

  buffer->buffer_size = size;
  buffer->last_filled = -1;
  buffer->last_emptied = -1;

  if (SEM_CREATE(buffer->frame_sem, size) != 0)
    goto error;

  return buffer;

error:
  if (buffer->buffer_list != NULL)
    oggplay_free (buffer->buffer_list);

  if (buffer->buffer_mirror != NULL)
    oggplay_free (buffer->buffer_mirror);

  oggplay_free (buffer);

  return NULL;
}

void
oggplay_buffer_shutdown(OggPlay *me, volatile OggPlayBuffer *vbuffer) {

  int i;
  int j;

  OggPlayBuffer *buffer = (OggPlayBuffer *)vbuffer;
  
  if (buffer == NULL) {
    return;
  }

  if (buffer->buffer_mirror != NULL) {
    for (i = 0; i < buffer->buffer_size; i++) {
      
      if (buffer->buffer_mirror[i] != NULL) {
        OggPlayCallbackInfo *ti = (OggPlayCallbackInfo *)buffer->buffer_mirror[i];
        for (j = 0; j < me->num_tracks; j++) {
          if ( (ti+j) != NULL) {
            oggplay_free((ti + j)->records);
          }
        }
        oggplay_free(ti);
      }
      
    }
    oggplay_free(buffer->buffer_mirror);
  }

  if (buffer->buffer_list != NULL) 
    oggplay_free(buffer->buffer_list);
    
  SEM_CLOSE(buffer->frame_sem);
  oggplay_free(buffer);
  buffer = NULL;
}

int
oggplay_buffer_is_full(volatile OggPlayBuffer *buffer) {

  return
  (
    (buffer == NULL) || (
      buffer->buffer_list[WRAP_INC(buffer->last_filled, buffer->buffer_size)]
      !=
      NULL
    )
  );

}

void
oggplay_buffer_set_last_data(OggPlay *me, volatile OggPlayBuffer *buffer) 
{

  int                   i;
  OggPlayCallbackInfo   *p;

  /*
   * we're at last data before we've even started!
   */
  if (buffer->last_filled == -1) {
    return;
  }

  p = (OggPlayCallbackInfo *)buffer->buffer_list[buffer->last_filled];

  for (i = 0; i < me->num_tracks; i++) {
    p->stream_info = OGGPLAY_STREAM_LAST_DATA;
    p++;
  }
}

int
oggplay_buffer_callback(OggPlay *me, int tracks,
                        OggPlayCallbackInfo **track_info, void *user) {

  int                   i;
  int                   j;
  int                   k;
  OggPlayDataHeader  ** headers;
  OggPlayBuffer       * buffer;
  OggPlayCallbackInfo * ptr = track_info[0];
  int                   required;

  if (me == NULL)
    return -1;

  buffer = (OggPlayBuffer *)me->buffer;

  if (buffer == NULL) {
    return -1;
  }

  SEM_WAIT(buffer->frame_sem);

  if (me->shutdown) {
    return -1;
  }

  /*
   * lock the item going into the buffer so that it doesn't get cleaned up
   */
  for (i = 0; i < tracks; i++) {
    headers = oggplay_callback_info_get_headers(track_info[i]);
    required = oggplay_callback_info_get_required(track_info[i]);
    for (j = 0; j < required; j++) {
      oggplay_callback_info_lock_item(headers[j]);
    }
  }

  /*
   * check for and clean up empties
   */
  for (k = 0; k < buffer->buffer_size; k++) {
    if
    (
      (buffer->buffer_list[k] == NULL)
      &&
      (buffer->buffer_mirror[k] != NULL)
    ) 
    {
      OggPlayCallbackInfo *ti = (OggPlayCallbackInfo *)buffer->buffer_mirror[k];
      for (i = 0; i < tracks; i++) {
        headers = oggplay_callback_info_get_headers(ti + i);
        required = oggplay_callback_info_get_required(ti + i);
        for (j = 0; j < required; j++) {
          oggplay_callback_info_unlock_item(headers[j]);
        }
        /* free these here, because we couldn't free them in
         * oggplay_callback_info_destroy for buffer mode
         */
        if ((ti + i) != NULL) {
          oggplay_free((ti + i)->records);
        }
      }
      oggplay_free(ti);
      buffer->buffer_mirror[k] = NULL;
    }
  }

  /*
   * replace the decode_data buffer for the next callback
   */
  me->callback_info = 
    (OggPlayCallbackInfo *)oggplay_calloc(me->num_tracks, sizeof (OggPlayCallbackInfo));
  if (me->callback_info == NULL)
    return -1;

  /*
   * fill both mirror and list, mirror first to avoid getting inconsistencies
   */

  buffer->last_filled = WRAP_INC(buffer->last_filled, buffer->buffer_size);

  /*
   * set the buffer pointer in the first record
   */
  ptr->buffer = buffer;

  buffer->buffer_mirror[buffer->last_filled] = ptr;
  buffer->buffer_list[buffer->last_filled] = ptr;


  if (oggplay_buffer_is_full(buffer)) {
    /*
     * user interrupt when we fill the buffer rather than when we have a
     * decoded frame and the buffer is already full
     */
    return -1;
  }

  return 0;
}

OggPlayCallbackInfo **
oggplay_buffer_retrieve_next(OggPlay *me) {

  OggPlayBuffer         * buffer = NULL;
  int                     next_loc;
  OggPlayCallbackInfo   * next_item;
  OggPlayCallbackInfo  ** return_val;
  int                     i;

  if (me == NULL) {
    return NULL;
  }

  buffer = (OggPlayBuffer *)me->buffer;

  if (buffer == NULL) {
    return NULL;
  }

  next_loc = WRAP_INC(buffer->last_emptied, buffer->buffer_size);

  if (buffer->buffer_list[next_loc] == NULL) {
    return NULL;
  }

  next_item = (OggPlayCallbackInfo*)buffer->buffer_list[next_loc];
  buffer->last_emptied = next_loc;

  return_val = oggplay_calloc(me->num_tracks, sizeof (OggPlayCallbackInfo *));
  if (return_val == NULL) {
    return NULL;
  }
  
  for (i = 0; i < me->num_tracks; i++) {
    return_val[i] = next_item + i;
  }

  return return_val;

}

OggPlayErrorCode
oggplay_buffer_release(OggPlay *me, OggPlayCallbackInfo **track_info) {

  OggPlayBuffer *buffer = NULL;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (track_info == NULL) {
    return E_OGGPLAY_OK;
  }

  buffer = (OggPlayBuffer *)track_info[0]->buffer;

  if (buffer == NULL) {
    return E_OGGPLAY_CALLBACK_MODE;
  }

  if (buffer->buffer_list[buffer->last_emptied] == NULL) {
    return E_OGGPLAY_UNINITIALISED;
  }

  if (track_info != NULL) {
    oggplay_free(track_info);
  }

  buffer->buffer_list[buffer->last_emptied] = NULL;

  SEM_SIGNAL(buffer->frame_sem);

  return E_OGGPLAY_OK;

}

OggPlayErrorCode
oggplay_use_buffer(OggPlay *me, int size) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->callback != NULL) {
    return E_OGGPLAY_CALLBACK_MODE;
  }

  if (me->buffer != NULL) {
    /*
     * we should check sizes, and maybe clear and reallocate the buffer?
     */
    return E_OGGPLAY_OK;
  }

  if( (me->buffer = oggplay_buffer_new_buffer(size)) == NULL)
    return E_OGGPLAY_OUT_OF_MEMORY;

  /*
   * if oggplay is already initialised, then prepare the buffer now
   */
  if (me->all_tracks_initialised) {
    oggplay_buffer_prepare(me);
  }

  return E_OGGPLAY_OK;
}

void
oggplay_buffer_prepare(OggPlay *me) {

  int i;

  if (me == NULL)
    return;

  oggplay_set_data_callback_force(me, &oggplay_buffer_callback, NULL);

  for (i = 0; i < me->num_tracks; i++) {
    if (oggplay_get_track_type(me, i) == OGGZ_CONTENT_THEORA) {
      oggplay_set_callback_num_frames(me, i, 1);
      break;
    }
  }

}
