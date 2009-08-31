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
 * oggplay_query.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 */

#include "oggplay_private.h"

int
oggplay_get_num_tracks (OggPlay * me) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->reader == NULL) {
    return E_OGGPLAY_BAD_READER;
  }

  if (me->all_tracks_initialised == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }

  return me->num_tracks;

}

OggzStreamContent
oggplay_get_track_type (OggPlay * me, int track_num) {

  if (me == NULL) {
    return (OggzStreamContent)E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->reader == NULL) {
    return (OggzStreamContent)E_OGGPLAY_BAD_READER;
  }

  if (me->all_tracks_initialised == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }

  if (track_num < 0 || track_num >= me->num_tracks) {
    return (OggzStreamContent)E_OGGPLAY_BAD_TRACK;
  }

  return (OggzStreamContent)me->decode_data[track_num]->content_type;
}

const char *
oggplay_get_track_typename (OggPlay * me, int track_num) {

  if (me == NULL) {
    return NULL;
  }

  if (me->reader == NULL) {
    return NULL;
  }

  if (me->all_tracks_initialised == 0) {
    return NULL;
  }

  if (track_num < 0 || track_num >= me->num_tracks) {
    return NULL;
  }

  return me->decode_data[track_num]->content_type_name;
}

OggPlayErrorCode
oggplay_set_track_active(OggPlay *me, int track_num) {

  ogg_int64_t p;

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->reader == NULL) {
    return E_OGGPLAY_BAD_READER;
  }

  if (me->all_tracks_initialised == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }

  if (track_num < 0 || track_num >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  /*
   * Skeleton tracks should not be set active - data in them should be queried
   * using alternative mechanisms (there is no concept of time-synced data
   * in a skeleton track)
   */
  if (me->decode_data[track_num]->content_type == OGGZ_CONTENT_SKELETON) {
    return E_OGGPLAY_TRACK_IS_SKELETON;
  }
  
  if (me->decode_data[track_num]->content_type == OGGZ_CONTENT_UNKNOWN) {
    return E_OGGPLAY_TRACK_IS_UNKNOWN;
  }

  /* there was an error while decoding the headers of this track! */
  if (me->decode_data[track_num]->initialised != 1) {
    return E_OGGPLAY_TRACK_UNINITIALISED;
  }

  if ((p = me->decode_data[track_num]->final_granulepos) != -1) {
    if (p * me->decode_data[track_num]->granuleperiod > me->target) {
      return E_OGGPLAY_TRACK_IS_OVER;
    }
  }

  if (me->decode_data[track_num]->active == 0) {
    me->decode_data[track_num]->active = 1;

    /*
     * CMML tracks aren't counted when deciding whether we've read enough data
     * from the stream.  This is because CMML data is not continuous, and
     * determining that we've read enough data from each other stream is enough
     * to determing that we've read any CMML data that is available.
     * This also applies to Kate streams.
     */
    if (me->decode_data[track_num]->content_type != OGGZ_CONTENT_CMML && me->decode_data[track_num]->content_type != OGGZ_CONTENT_KATE) {
      me->active_tracks ++;
    }
  }

  return E_OGGPLAY_OK;

}

OggPlayErrorCode
oggplay_set_track_inactive(OggPlay *me, int track_num) {

  if (me == NULL) {
    return E_OGGPLAY_BAD_OGGPLAY;
  }

  if (me->reader == NULL) {
    return E_OGGPLAY_BAD_READER;
  }

  if (me->all_tracks_initialised == 0) {
    return E_OGGPLAY_UNINITIALISED;
  }

  if (track_num < 0 || track_num >= me->num_tracks) {
    return E_OGGPLAY_BAD_TRACK;
  }

  if (me->decode_data[track_num]->content_type == OGGZ_CONTENT_SKELETON) {
    return E_OGGPLAY_TRACK_IS_SKELETON;
  }

  if (me->decode_data[track_num]->content_type == OGGZ_CONTENT_UNKNOWN) {
    return E_OGGPLAY_TRACK_IS_UNKNOWN;
  }

  if (me->decode_data[track_num]->active == 1) {
    me->decode_data[track_num]->active = 0;

    /*
     * see above comment in oggplay_set_track_active
     */
    if (me->decode_data[track_num]->content_type != OGGZ_CONTENT_CMML && me->decode_data[track_num]->content_type != OGGZ_CONTENT_KATE) {
      me->active_tracks --;
    }
  }

  return E_OGGPLAY_OK;
}
