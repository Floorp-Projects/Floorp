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
 * oggplay_callback_info.c
 * 
 * Shane Stephens <shane.stephens@annodex.net>
 */
#include "oggplay_private.h"
#include <stdlib.h>

extern void _print_list(char *name, OggPlayDataHeader *p);

static void
clear_callback_info (OggPlay *me, OggPlayCallbackInfo ***info) {
  int i;
  
  for (i = 0; i < me->num_tracks; ++i) {
    if (((*info)[i] != NULL) && ((*info)[i]->records != NULL)) {
        oggplay_free ((*info)[i]->records);
    }
  }
  oggplay_free (*info);
  *info = NULL;
}

int
oggplay_callback_info_prepare(OggPlay *me, OggPlayCallbackInfo ***info) {

  int i;
  int tcount = 0;
  
  int         added_required_record   = me->num_tracks;
  ogg_int64_t diff;
  ogg_int64_t latest_first_record     = 0x0LL;
  //ogg_int64_t lpt = 0;
 
  /*
   * allocate the structure for return to the user
   */
  (*info) = oggplay_calloc (me->num_tracks, sizeof (OggPlayCallbackInfo *));
  if ((*info) == NULL)
    return E_OGGPLAY_OUT_OF_MEMORY;
  
  /*
   * fill in each active track.  Leave gaps for inactive tracks.
   */
  for (i = 0; i < me->num_tracks; i++) {
    OggPlayDecode       * track = me->decode_data[i];
    OggPlayCallbackInfo * track_info = me->callback_info + i;
    size_t                count = 0;
    OggPlayDataHeader   * p;
    OggPlayDataHeader   * q = NULL;
    
    (*info)[i] = track_info;

#ifdef HAVE_TIGER
    /* not so nice to have this here, but the tiger_renderer needs updating regularly
     * as some items may be animated, so would yield data without the stream actually
     * receiving any packets.
     * In addition, Kate streams can now be overlayed on top of a video, so this needs
     * calling to render the overlay.
     * FIXME: is this the best place to put this ? Might do too much work if the info
     * is going to be destroyed ?
     */
    if (track->content_type == OGGZ_CONTENT_KATE) {
      OggPlayKateDecode *decode = (OggPlayKateDecode *)track;
      OggPlayCallbackInfo * video_info = NULL;
      if (decode->overlay_dest >= 0)
        video_info = me->callback_info + decode->overlay_dest;
      oggplay_data_update_tiger(decode, track->active, me->target, video_info);
    }
#endif

    /*
     * this track is inactive and has no data - create an empty record
     * for it
     */
    if (track->active == 0 && track->data_list == NULL) {
      track_info->data_type = OGGPLAY_INACTIVE;
      track_info->available_records = track_info->required_records = 0;
      track_info->records = NULL;
      track_info->stream_info = OGGPLAY_STREAM_UNINITIALISED;
      added_required_record --;
      continue;
    }
 
    /*
     * find the first not presented packet of data, count the total number that
     * have not been presented
     */
    for (p = track->data_list; p != NULL; p = p->next) {
      if (!p->has_been_presented) {
        if (q == NULL) {
          q = p;
        }
        
        /* check for overflow */
        if 
        (
          oggplay_check_add_overflow (count, 1, &count)
          == 
          E_OGGPLAY_TYPE_OVERFLOW
        )
        {
          clear_callback_info (me, info);
          return E_OGGPLAY_TYPE_OVERFLOW;
        }
      }
    }
  
    /*
     * tcount is set if any of the tracks have unpresented data
     */
    if (count > 0) {
      tcount = 1;

      /*
       * set this track's StreamState.  If the track isn't active and there's
       * only one timestamp's worth of data in the data list, then this is
       * the last data!
       */
      if 
      (
        track->active == 0 
        && 
        (
          track->end_of_data_list->presentation_time
          <=
          me->target + track->offset
        )
      ) 
      {
        track_info->stream_info = OGGPLAY_STREAM_LAST_DATA;
      } else {
        track_info->stream_info = track->stream_info;
      }

    } else {
      track_info->stream_info = OGGPLAY_STREAM_UNINITIALISED;
    }

    if ((count+1) < count) {
      clear_callback_info (me, info);
      return E_OGGPLAY_TYPE_OVERFLOW;
    }
    /* null-terminate the record list for the python interface */
    track_info->records = 
      oggplay_calloc ((count+1), sizeof (OggPlayDataHeader *));
    if (track_info->records == NULL) {
      clear_callback_info (me, info);
      return E_OGGPLAY_OUT_OF_MEMORY;
    }

    track_info->records[count] = NULL;

    track_info->available_records = count;
    track_info->required_records = 0;

    track_info->data_type = track->decoded_type;
 
    count = 0;
    for (p = q; p != NULL; p = p->next) {
      if (!p->has_been_presented) {
        track_info->records[count++] = p;
        if (p->presentation_time <= me->target + track->offset) {
          track_info->required_records++;
          p->has_been_presented = 1;
          //lpt = p->presentation_time;
        }
      }
    }
     
    if (track_info->required_records > 0) {
      /*
       * if the StreamState is FIRST_DATA then update it to INITIALISED, 
       * as we've marked the first data instance
       */
      if 
      (
        track->stream_info == OGGPLAY_STREAM_FIRST_DATA
        ||
        track->stream_info == OGGPLAY_STREAM_JUST_SEEKED
      ) 
      {
        track->stream_info = OGGPLAY_STREAM_INITIALISED;
      }
 
    }

    /*
    printf("%d: %d/%d\t", i, 
                    track_info->required_records, count);
    
    if (q != NULL) {
      printf("fst: %lld lst: %lld sz: %lld pt: %lld\n", 
                      q->presentation_time >> 32, lpt >> 32, 
                      (lpt - q->presentation_time) >> 32, 
                      me->presentation_time >> 32);
    }
    */

    /*
     * this statement detects if this track needs records but has none.
     * We need to be careful - there are 2 cases where this could happen.  The
     * first is where the presentation time is less than it should be, and
     * there is no data available between now and the target.  In this case
     * we want to set added_required_record to 0 and trigger the presentation
     * time update code below.
     *                            <-------data----------...
     * ^      ^                         ^
     * pt     target                    current_loc
     *
     * The second case occurs when the packet from the last timestamp contained
     * so much data that there *is* none in this timestamp.  In this case we 
     * don't want to set added_required_record to 0.
     *
     *    <----------data---------------|--------...
     *    <-timeslice1-><-timeslice2-><-timeslice3->
     * ^                              ^                    ^
     * pt                             target               current_loc
     *
     * How do we discriminate between these two cases?  We assume the pt update
     * needs to be explicitly required (e.g. by seeking or start of movie), and
     * create a new member in the player struct called pt_update_valid
     */
     // TODO: I don't think that pt_update_valid is necessary any more, as this will only
     // trigger now if there's no data in *ANY* of the tracks. Hence the audio timeslice case
     // doesn't apply.
    if 
    (
      track->decoded_type == OGGPLAY_CMML 
      ||
      track->decoded_type == OGGPLAY_KATE // TODO: check this is the right thing to do
      ||
      (
        track_info->required_records == 0
        &&
        track->active == 1
        && 
        me->pt_update_valid
      )
    ) {
      added_required_record --;
    }

  } /* end of for loop, that fills each track */
 
   me->pt_update_valid = 0;
    
  //printf("\n");

  /*
   * there are no required records!  This means that we need to advance the
   * target to the period before the first actual record - the bitstream has
   * lied about the presence of data here.
   *
   * This happens for example with some Annodex streams (a bug in libannodex
   * causes incorrect timestamping)
   *
   * What we actually need is the first timestamp *just before* a timestamp
   * with valid data on all active tracks that are not CMML tracks.
   */
  latest_first_record = 0x0LL;
  if (tcount > 0 && added_required_record == 0) {
    for (i = 0; i < me->num_tracks; i++) {
      OggPlayCallbackInfo * track_info = me->callback_info + i;
      if (track_info->data_type == OGGPLAY_CMML || track_info->data_type == OGGPLAY_KATE) {
        continue;
      }
      if (track_info->available_records > 0) {
        if (track_info->records[0]->presentation_time > latest_first_record) {
          latest_first_record = track_info->records[0]->presentation_time;
        }
      }
    }
    /*
     * update target to the time-period before the first record.  This is
     * independent of the offset (after all, we want to maintain synchronisation
     * between the streams).
     */
    diff = latest_first_record - me->target;
    diff = (diff / me->callback_period) * me->callback_period;
    me->target += diff + me->callback_period;


    /*
     * update the presentation_time to the latest_first_record.  This ensures 
     * that we don't play material for timestamps that only exist in one track.
     */
    me->presentation_time = me->target - me->callback_period;

    /*
     * indicate that we need to go through another round of fragment collection
     * and callback creation
     */
    for (i = 0; i < me->num_tracks; i++) {
      if ((*info)[i]->records != NULL) 
        oggplay_free((*info)[i]->records);
    }
    oggplay_free(*info);
    (*info) = NULL;

  }

  if (tcount == 0) {
    for (i = 0; i < me->num_tracks; i++) {
      if ((*info)[i]->records != NULL) 
        oggplay_free((*info)[i]->records);
    }
    oggplay_free(*info);
    (*info) = NULL;
  }

  return me->num_tracks;
  
}


void
oggplay_callback_info_destroy(OggPlay *me, OggPlayCallbackInfo **info) {

  int                   i;
  OggPlayCallbackInfo * p;

  for (i = 0; i < me->num_tracks; i++) {
    p = info[i];
    if (me->buffer == NULL && p->records != NULL)
      oggplay_free(p->records);
  }

  oggplay_free(info);

}

OggPlayDataType
oggplay_callback_info_get_type(OggPlayCallbackInfo *info) {

  if (info == NULL) {
    return (OggPlayDataType)E_OGGPLAY_BAD_CALLBACK_INFO;
  }

  return info->data_type;

}

int
oggplay_callback_info_get_available(OggPlayCallbackInfo *info) {

  if (info == NULL) {
    return E_OGGPLAY_BAD_CALLBACK_INFO;
  }

  return info->available_records;

}

int
oggplay_callback_info_get_required(OggPlayCallbackInfo *info) {

  if (info == NULL) {
    return E_OGGPLAY_BAD_CALLBACK_INFO;
  }

  return info->required_records;

}

OggPlayStreamInfo
oggplay_callback_info_get_stream_info(OggPlayCallbackInfo *info) {

  if (info == NULL) {
    return E_OGGPLAY_BAD_CALLBACK_INFO;
  }

  return info->stream_info;
}

OggPlayDataHeader **
oggplay_callback_info_get_headers(OggPlayCallbackInfo *info) {

  if (info == NULL) {
    return NULL;
  }

  return info->records;

}

/** 
 * Returns number of samples in the record 
 * Note: the resulting data include samples for all audio channels */
ogg_int64_t
oggplay_callback_info_get_record_size(OggPlayDataHeader *header) {

  if (header == NULL) {
    return 0;
  }

  return header->samples_in_record;

}

void
oggplay_callback_info_lock_item(OggPlayDataHeader *header) {

  if (header == NULL) {
    return;
  }

  header->lock += 1;

}

void
oggplay_callback_info_unlock_item(OggPlayDataHeader *header) {
  
  if (header == NULL) {
    return;
  }

  header->lock -= 1;
}

long
oggplay_callback_info_get_presentation_time(OggPlayDataHeader *header) {

  if (header == NULL) {
    return -1;
  }

  return OGGPLAY_TIME_FP_TO_INT(header->presentation_time);
}

OggPlayVideoData *
oggplay_callback_info_get_video_data(OggPlayDataHeader *header) {

  if (header == NULL) {
    return NULL;
  }

  return &((OggPlayVideoRecord *)header)->data;

}

OggPlayOverlayData *
oggplay_callback_info_get_overlay_data(OggPlayDataHeader *header) {

  if (header == NULL) {
    return NULL;
  }

  return &((OggPlayOverlayRecord *)header)->data;

}

OggPlayAudioData *
oggplay_callback_info_get_audio_data(OggPlayDataHeader *header) {

  if (header == NULL) {
    return NULL;
  }

  return (OggPlayAudioData *)((OggPlayAudioRecord *)header)->data;
}

OggPlayTextData *
oggplay_callback_info_get_text_data(OggPlayDataHeader *header) {

  if (header == NULL) {
    return NULL;
  }

  return ((OggPlayTextRecord *)header)->data;

}

