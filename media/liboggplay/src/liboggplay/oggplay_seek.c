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
 * oggplay_enums.h
 *
 * Shane Stephens <shane.stephens@annodex.net>
 */

#include "oggplay_private.h"

OggPlayErrorCode
oggplay_seek(OggPlay *me, ogg_int64_t milliseconds) {

  ogg_int64_t           eof;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (milliseconds < 0) {
    return E_OGGPLAY_CANT_SEEK;
  }

  eof = oggplay_get_duration(me);
  if (eof > -1 && milliseconds > eof) {
    return E_OGGPLAY_CANT_SEEK;
  }

  if (me->reader->seek != NULL) {
    if
    (
      me->reader->seek(me->reader, me->oggz, milliseconds)
      ==
      E_OGGPLAY_CANT_SEEK
    )
    {
      return E_OGGPLAY_CANT_SEEK;
    }
  } else {
    if (oggz_seek_units(me->oggz, milliseconds, SEEK_SET) == -1) {
      return E_OGGPLAY_CANT_SEEK;
    }
  }

  oggplay_seek_cleanup(me, milliseconds);

  return E_OGGPLAY_OK;

}

void
oggplay_seek_cleanup(OggPlay* me, ogg_int64_t milliseconds)
{

  OggPlaySeekTrash    * trash;
  OggPlaySeekTrash   ** p;
  OggPlayDataHeader  ** end_of_list_p;
  int                   i;

  /*
   * first, create a trash object to store the context that we want to
   * delete but can't until the presentation thread is no longer using it -
   * this will occur as soon as the thread calls oggplay_buffer_release_next
   */

  trash = calloc(sizeof(OggPlaySeekTrash), 1);

  /*
   * store the old buffer in it next.
   */
  trash->old_buffer = (OggPlayBuffer *)me->buffer;

  /*
   * replace the buffer with a new one.  From here on, the presentation thread
   * will start using this buffer instead.
   */
  me->buffer = oggplay_buffer_new_buffer(me->buffer->buffer_size);

  /*
   * strip all of the data packets out of the streams and put them into the
   * trash.  We can free the untimed packets immediately - they are USELESS
   * SCUM OF THE EARTH (and also unreferenced by the buffer).
   */
  end_of_list_p = &trash->old_data;
  for (i = 0; i < me->num_tracks; i++) {
    OggPlayDecode *track = me->decode_data[i];
    if (track->data_list != NULL) {
      *(end_of_list_p) = track->data_list;
      end_of_list_p = &(track->end_of_data_list->next);
      oggplay_data_free_list(track->untimed_data_list);
    }
    track->data_list = track->end_of_data_list = NULL;
    track->untimed_data_list = NULL;
    track->current_loc = -1;
    track->stream_info = OGGPLAY_STREAM_JUST_SEEKED;
  }

  /*
   * set the presentation time
   */
  me->presentation_time = milliseconds;
  me->target = me->callback_period - 1;
  me->pt_update_valid = 1;

  trash->next = NULL;

  p = &(me->trash);
  while (*p != NULL) {
    p = &((*p)->next);
  }

  *p = trash;
}

void
oggplay_take_out_trash(OggPlay *me, OggPlaySeekTrash *trash) {

  OggPlaySeekTrash *p = NULL;

  for (; trash != NULL; trash = trash->next) {

    oggplay_buffer_shutdown(me, trash->old_buffer);
    oggplay_data_free_list(trash->old_data);
    if (p != NULL) {
      free(p);
    }
    p = trash;
  }

  if (p != NULL) {
    free(p);
  }
}
