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
 * oggplay_data.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 */

#include "oggplay_private.h"
#include "oggplay/oggplay_callback_info.h"

#include <stdlib.h>
#include <string.h>

#if HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if LONG_MAX==2147483647L
#define PRId64 "lld"
#else
#define PRId64 "ld"
#endif
#endif

/*
 * the normal lifecycle for a frame is:
 *
 * (1) frame gets decoded and added to list with a locking value of 1
 * (2) frame gets delivered to user
 * (3) frame becomes out-of-date (its presentation time expires) and its
 *      lock is decremented
 * (4) frame is removed from list and freed
 *
 * This can be modified by:
 * (a) early consumption by user (user calls oggplay_mark_record_consumed)
 * (b) frame locking by user (user calls oggplay_mark_record_locked) and
 *     subsequent unlocking (user calls oggplay_mark_record_consumed)
 */

void
oggplay_data_initialise_list (OggPlayDecode *decode) {

  if (decode == NULL) {
    return;
  }
  
  decode->data_list = decode->end_of_data_list = NULL;
  decode->untimed_data_list = NULL;

}

/*
 * helper function to append data packets to end of data_list
 */
void
oggplay_data_add_to_list_end(OggPlayDecode *decode, OggPlayDataHeader *data) {

  if (decode == NULL) {
    return;
  }
  
  data->next = NULL;

  if (decode->data_list == NULL) {
    decode->data_list = data;
    decode->end_of_data_list = data;
  } else {
    decode->end_of_data_list->next = data;
    decode->end_of_data_list = data;
  }

}

/*
 * helper function to append data packets to front of data_list
 */
void
oggplay_data_add_to_list_front(OggPlayDecode *decode, OggPlayDataHeader *data) {
  if (decode == NULL) {
    return;
  }
  
  if (decode->data_list == NULL) {
    decode->data_list = decode->end_of_data_list = data;
    data->next = NULL;
  } else {
    data->next = decode->data_list;
    decode->data_list = data;
  }
}

void
_print_list(char *name, OggPlayDataHeader *p) {
    printf("%s: ", name);
    for (; p != NULL; p = p->next) {
      printf("%"PRId64"[%d]", OGGPLAY_TIME_FP_TO_INT (p->presentation_time), p->lock);
      if (p->next != NULL) printf("->");
    }
    printf("\n");
}


void
oggplay_data_add_to_list (OggPlayDecode *decode, OggPlayDataHeader *data) {

  /*
   * if this is a packet with an unknown display time, prepend it to
   * the untimed_data_list for later timestamping.
   */

  ogg_int64_t samples_in_next_in_list;

  if ((decode == NULL) || (data == NULL)) {
    return;
  }

  //_print_list("before", decode->data_list);
  //_print_list("untimed before", decode->untimed_data_list);

  if (data->presentation_time == -1) {
    data->next = decode->untimed_data_list;
    decode->untimed_data_list = data;
  } else {
    /*
     * process the untimestamped data into the timestamped data list.
     *
     * First store any old data.
     */
    ogg_int64_t presentation_time         = data->presentation_time;
    samples_in_next_in_list               = data->samples_in_record;


    while (decode->untimed_data_list != NULL) {
      OggPlayDataHeader *untimed = decode->untimed_data_list;

      presentation_time -=
                samples_in_next_in_list * decode->granuleperiod;
      untimed->presentation_time = presentation_time;
      decode->untimed_data_list = untimed->next;
      samples_in_next_in_list = untimed->samples_in_record;

      if (untimed->presentation_time >= decode->player->presentation_time) {
        oggplay_data_add_to_list_front(decode, untimed);
      } else {
        oggplay_free(untimed);
      }

    }

    oggplay_data_add_to_list_end(decode, data);

    /*
     * if the StreamInfo is still at uninitialised, then this is the first
     * meaningful data packet!  StreamInfo will be updated to
     * OGGPLAY_STREAM_INITIALISED in oggplay_callback_info.c as part of the
     * callback process.
     */
    if (decode->stream_info == OGGPLAY_STREAM_UNINITIALISED) {
      decode->stream_info = OGGPLAY_STREAM_FIRST_DATA;
    }

  }

  //_print_list("after", decode->data_list);
  //_print_list("untimed after", decode->untimed_data_list);

}

void
oggplay_data_free_list(OggPlayDataHeader *list) {
  OggPlayDataHeader *p;

  while (list != NULL) {
    p = list;
    list = list->next;
    oggplay_free(p);
  }
}

void
oggplay_data_shutdown_list (OggPlayDecode *decode) {

  if (decode == NULL) {
    return;
  }
  
  oggplay_data_free_list(decode->data_list);
  oggplay_data_free_list(decode->untimed_data_list);

}

/*
 * this function removes any displayed, unlocked frames from the list.
 *
 * this function also removes any undisplayed frames that are before the
 * global presentation time.
 */
void
oggplay_data_clean_list (OggPlayDecode *decode) {

  ogg_int64_t         target;
  OggPlayDataHeader * header = NULL;
  OggPlayDataHeader * p      = NULL;
  
  if (decode == NULL) {
    return;
  }
  header = decode->data_list;
  target = decode->player->target;
  
  while (header != NULL) {
    if
    (
      header->lock == 0
      &&
      (
        (
          (header->presentation_time < (target + decode->offset))
          &&
          header->has_been_presented
        )
        ||
        (
          (header->presentation_time < decode->player->presentation_time)
        )
      )

    )
    {
      if (p == NULL) {
        decode->data_list = decode->data_list->next;
        if (decode->data_list == NULL)
          decode->end_of_data_list = NULL;
        oggplay_free (header);
        header = decode->data_list;
      } else {
        if (header->next == NULL)
          decode->end_of_data_list = p;
        p->next = header->next;
        oggplay_free (header);
        header = p->next;
      }
    } else {
      p = header;
      header = header->next;
    }
  }
}

void
oggplay_data_initialise_header (const OggPlayDecode *decode,
                                OggPlayDataHeader *header) {
  
  if ((decode == NULL) || (header == NULL)) {
    return;
  }
  
  /*
   * the frame is not cleaned until its presentation time has passed.  We'll
   * check presentation times in oggplay_data_clean_list.
   */
  header->lock = 0;
  header->next = NULL;
  header->presentation_time = decode->current_loc;
  header->has_been_presented = 0;
}

OggPlayErrorCode
oggplay_data_handle_audio_data (OggPlayDecode *decode, void *data,
                                long samples, size_t samplesize) {

  int                   num_channels, ret;
  size_t                record_size = sizeof(OggPlayAudioRecord);
  long                  samples_size;
  OggPlayAudioRecord  * record = NULL;

  num_channels = ((OggPlayAudioDecode *)decode)->sound_info.channels;
  
  /* check for possible integer overflows....*/
  if ((samples < 0) || (num_channels < 0)) {
    return E_OGGPLAY_TYPE_OVERFLOW;
  }

  ret = oggplay_mul_signed_overflow (samples, num_channels, &samples_size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }

  ret = oggplay_mul_signed_overflow (samples_size, samplesize, &samples_size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }

  ret = oggplay_check_add_overflow (record_size, samples_size, &record_size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }

  /* try to allocate the memory for the record */
  record = (OggPlayAudioRecord*)oggplay_calloc(record_size, 1);
  if (record == NULL) {
    return E_OGGPLAY_OUT_OF_MEMORY;
  }

  /* initialise the header of OggPlayAudioRecord struct */
  oggplay_data_initialise_header(decode, &(record->header));

  record->header.samples_in_record = samples;

  record->data = (void *)(record + 1);
  
  /* copy the received data - the header has been initialised! */
  memcpy (record->data, data, samples_size);
  /*
  printf("[%f%f%f]\n", ((float *)record->data)[0], ((float *)record->data)[1],
                    ((float *)record->data)[2]);
  */
  oggplay_data_add_to_list(decode, &(record->header));
  
  return E_OGGPLAY_CONTINUE;
}

OggPlayErrorCode
oggplay_data_handle_cmml_data(OggPlayDecode *decode, 
                              unsigned char *data, 
                              long size) {

  OggPlayTextRecord * record = NULL;
  size_t              record_size = sizeof(OggPlayTextRecord);

  /* check that the size we want to allocate doesn't overflow */
  if ((size < 0) || (size+1 < 0)) {
    return E_OGGPLAY_TYPE_OVERFLOW;
  }
  size += 1;
  
  if 
  (
    oggplay_check_add_overflow (record_size, size, &record_size)
    == 
    E_OGGPLAY_TYPE_OVERFLOW
  ) 
  {
    return E_OGGPLAY_TYPE_OVERFLOW;
  }
  
  /* allocate the memory for the record */
  record = (OggPlayTextRecord*)oggplay_calloc (record_size, 1);
  if (record == NULL) {
    return E_OGGPLAY_OUT_OF_MEMORY;
  }

  /* initialise the record's header */
  oggplay_data_initialise_header(decode, &(record->header));

  record->header.samples_in_record = 1;
  record->data = (char *)(record + 1);

  /* copy the data */
  memcpy(record->data, data, size);
  record->data[size] = '\0';

  oggplay_data_add_to_list(decode, &(record->header));

  return E_OGGPLAY_CONTINUE;
}

static int
get_uv_offset(OggPlayTheoraDecode *decode, yuv_buffer *buffer)
{
  int xo=0, yo = 0;
  if (decode->y_width != 0 &&
      decode->uv_width != 0 &&
      decode->y_width/decode->uv_width != 0) {
    xo = (decode->video_info.offset_x/(decode->y_width/decode->uv_width));
  }
  if (decode->y_height != 0 &&
      decode->uv_height != 0 &&
      decode->y_height/decode->uv_height != 0) {
    yo = (buffer->uv_stride)*(decode->video_info.offset_y/(decode->y_height/decode->uv_height));
  }
  return xo + yo;
}

int
oggplay_data_handle_theora_frame (OggPlayTheoraDecode *decode,
                                  const yuv_buffer *buffer) {

  size_t                size = sizeof (OggPlayVideoRecord);
  int                   i, ret;
  long                  y_size, uv_size, y_offset, uv_offset;
  unsigned char       * p;
  unsigned char       * q;
  unsigned char       * p2;
  unsigned char       * q2;
  OggPlayVideoRecord  * record;
  OggPlayVideoData    * data;

  /* check for possible integer overflows */
  ret = 
    oggplay_mul_signed_overflow (buffer->y_height, buffer->y_stride, &y_size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }

  ret = 
    oggplay_mul_signed_overflow (buffer->uv_height, buffer->uv_stride, &uv_size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }

  ret = oggplay_mul_signed_overflow (uv_size, 2, &uv_size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }

  if (buffer->y_stride < 0) {
    y_size  *= -1;
    uv_size *= -1;
  }

  ret = oggplay_check_add_overflow (size, y_size, &size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }
  
  ret = oggplay_check_add_overflow (size, uv_size, &size);
  if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
    return ret;
  }

  /*
   * we need to set the output strides to the input widths because we are
   * trying not to pass negative output stride issues on to the poor user.
   */
  record = (OggPlayVideoRecord*)oggplay_malloc (size);

  if (record == NULL) {
    return E_OGGPLAY_OUT_OF_MEMORY;
  }
  
  record->header.samples_in_record = 1;
  data = &(record->data);

  data->y = (unsigned char *)(record + 1);
  data->u = data->y + (decode->y_stride * decode->y_height);
  data->v = data->u + (decode->uv_stride * decode->uv_height);

  /*
   * *grumble* theora plays silly buggers with pointers so we need to do
   * a row-by-row copy (stride may be negative)
   */
  y_offset = (decode->video_info.offset_x&~1) 
              + buffer->y_stride*(decode->video_info.offset_y&~1);
  p = data->y;
  q = buffer->y + y_offset;
  for (i = 0; i < decode->y_height; i++) {
    memcpy(p, q, decode->y_width);
    p += decode->y_width;
    q += buffer->y_stride;
  }

  uv_offset = get_uv_offset(decode, buffer);
  p = data->u;
  q = buffer->u + uv_offset;
  p2 = data->v;
  q2 = buffer->v + uv_offset;
  for (i = 0; i < decode->uv_height; i++) {
    memcpy(p, q, decode->uv_width);
    memcpy(p2, q2, decode->uv_width);
    p += decode->uv_width;
    p2 += decode->uv_width;
    q += buffer->uv_stride;
    q2 += buffer->uv_stride;
  }

  /* if we're to send RGB video, convert here */
  if (decode->convert_to_rgb) {
    OggPlayYUVChannels      yuv;
    OggPlayRGBChannels      rgb;
    OggPlayOverlayRecord  * orecord;
    OggPlayOverlayData    * odata;
    long                    overlay_size;

    yuv.ptry = data->y;
    yuv.ptru = data->u;
    yuv.ptrv = data->v;
    yuv.y_width = decode->y_width;
    yuv.y_height = decode->y_height;
    yuv.uv_width = decode->uv_width;
    yuv.uv_height = decode->uv_height;

    size = sizeof(OggPlayOverlayRecord);
    /* check for possible integer oveflows */
    ret = oggplay_mul_signed_overflow(decode->y_width, decode->y_height, 
                                      &overlay_size);
    if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
      return ret;
    }
    
    ret = oggplay_mul_signed_overflow(overlay_size, 4, &overlay_size);
    if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
      return ret;
    }

    ret =  oggplay_check_add_overflow (size, overlay_size, &size);
    if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
      return ret;
    }

    /* allocate memory for the overlay record */
    orecord = (OggPlayOverlayRecord*) oggplay_malloc (size);
    if (orecord != NULL) {
      oggplay_data_initialise_header((OggPlayDecode *)decode, &(orecord->header));
      orecord->header.samples_in_record = 1;
      odata = &(orecord->data);

      rgb.ptro = (unsigned char*)(orecord+1);
      rgb.rgb_width = yuv.y_width;
      rgb.rgb_height = yuv.y_height;

      if (!decode->swap_rgb) {
        oggplay_yuv2bgra(&yuv, &rgb);
      } else {
        oggplay_yuv2rgba(&yuv, &rgb);
      }
      
      odata->rgb = rgb.ptro;
      odata->rgba = NULL;
      odata->width = rgb.rgb_width;
      odata->height = rgb.rgb_height;
      odata->stride = rgb.rgb_width*4;

      oggplay_free(record);
    
      oggplay_data_add_to_list((OggPlayDecode *)decode, &(orecord->header));
    } else {
      /* memory allocation failed! */
      oggplay_free (record);
      
      return E_OGGPLAY_OUT_OF_MEMORY;
    }
  }
  else {
    oggplay_data_initialise_header((OggPlayDecode *)decode, &(record->header));
    oggplay_data_add_to_list((OggPlayDecode *)decode, &(record->header));
  }
  
  return E_OGGPLAY_CONTINUE;
}

#ifdef HAVE_KATE
OggPlayErrorCode
oggplay_data_handle_kate_data(OggPlayKateDecode *decode, const kate_event *ev) {

  OggPlayTextRecord * record = NULL;
  size_t              rec_size = sizeof(OggPlayTextRecord);
  
  if (decode == NULL) {
    return -1; 
  }
#ifdef HAVE_TIGER
  tiger_renderer_add_event(decode->tr, ev->ki, ev);

  if (decode->use_tiger) {
    /* if rendering with Tiger, we don't add an event, a synthetic one will be
       generated each "tick" with an updated tracker state */
  }
  else
#endif
  {
    /* check for integer overflow */
    if 
    ( 
      oggplay_check_add_overflow (rec_size, ev->len0, &rec_size)
      == 
      E_OGGPLAY_TYPE_OVERFLOW
    )
    {
      return E_OGGPLAY_TYPE_OVERFLOW;
    }
    
    record = (OggPlayTextRecord*)oggplay_calloc (rec_size, 1);
    if (!record) {
      return E_OGGPLAY_OUT_OF_MEMORY;
    }

    oggplay_data_initialise_header(&decode->decoder, &(record->header));

    //record->header.presentation_time = (ogg_int64_t)(ev->start_time*1000);
    record->header.samples_in_record = (ev->end_time-ev->start_time)*1000;
    record->data = (char *)(record + 1);

    memcpy(record->data, ev->text, ev->len0);

    oggplay_data_add_to_list(&decode->decoder, &(record->header));
  }
  
  return E_OGGPLAY_CONTINUE;
}
#endif

#ifdef HAVE_TIGER
OggPlayErrorCode
oggplay_data_update_tiger(OggPlayKateDecode *decode, int active, ogg_int64_t presentation_time, OggPlayCallbackInfo *info) {

  OggPlayOverlayRecord  * record = NULL;
  OggPlayOverlayData    * data = NULL;
  size_t                size = sizeof (OggPlayOverlayRecord);
  int                   track = active && decode->use_tiger;
  int                   ret;
  kate_float            t = OGGPLAY_TIME_FP_TO_INT(presentation_time) / 1000.0f;

  if (!decode->decoder.initialised) return -1;

  if (track) {
    if (info) {
      if (info->required_records>0) {
        OggPlayDataHeader *header = info->records[0];
        data = (OggPlayOverlayData*)(header+1);
        if (decode->tr && data->rgb) {
#if WORDS_BIGENDIAN || IS_BIG_ENDIAN
          tiger_renderer_set_buffer(decode->tr, data->rgb, data->width, data->height, data->stride, 0);
#else
          tiger_renderer_set_buffer(decode->tr, data->rgb, data->width, data->height, data->stride, 1);
#endif
        }
        else {
          /* we're supposed to overlay on a frame, but the available frame has no RGB buffer */
          /* fprintf(stderr,"no RGB buffer found for video frame\n"); */
          return -1;
        }
      }
      else {
        /* we're supposed to overlay on a frame, but there is no frame available */
        /* fprintf(stderr,"no video frame to overlay on\n"); */
        return -1;
      }
    }
    else {
      // TODO: some way of knowing the size of the video we'll be drawing onto, if any
      int width = decode->k_state.ki->original_canvas_width;
      int height = decode->k_state.ki->original_canvas_height;
      long overlay_size;
      if (width <= 0 || height <= 0) {
        /* some default resolution if we're not overlaying onto a video and the canvas size is unknown */
        if (decode->default_width > 0 && decode->default_height > 0) {
          width = decode->default_width;
          height = decode->default_height;
        }
        else {
          width = 640;
          height = 480;
        }
      }
      /* check for integer overflow */
      ret = oggplay_mul_signed_overflow (width, height, &overlay_size);
      if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
        return ret;
      }
      
      ret = oggplay_mul_signed_overflow (overlay_size, 4, &overlay_size);
      if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
        return E_OGGPLAY_TYPE_OVERFLOW;
      }
      
      ret = oggplay_check_add_overflow (size, overlay_size, &size);
      if (ret == E_OGGPLAY_TYPE_OVERFLOW) {
        return E_OGGPLAY_TYPE_OVERFLOW;
      }

      record = (OggPlayOverlayRecord*)oggplay_calloc (1, size);
      if (!record)
        return E_OGGPLAY_OUT_OF_MEMORY;

      record->header.samples_in_record = 1;
      data= &(record->data);
      oggplay_data_initialise_header((OggPlayDecode *)decode, &(record->header));

      data->rgba = (unsigned char*)(record+1);
      data->rgb = NULL;
      data->width = width;
      data->height = height;
      data->stride = width*4;

      if (decode->tr && data->rgba) {
        tiger_renderer_set_buffer(decode->tr, data->rgba, data->width, data->height, data->stride, decode->swap_rgb);
      }

      oggplay_data_add_to_list(&decode->decoder, &(record->header));
      record->header.presentation_time=presentation_time;
    }
  }

  if (decode->tr) {
    tiger_renderer_update(decode->tr, t, track);
  }

  if (track) {
    /* buffer was either calloced, so already cleared, or already filled with video, so no clearing */
    if (decode->tr) {
      tiger_renderer_render(decode->tr);
    }
  }
  
  return E_OGGPLAY_CONTINUE;
}
#endif

