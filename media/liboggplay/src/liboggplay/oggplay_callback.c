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
 * oggplay_callback.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 */
#include "oggplay_private.h"

#define TIME_THEORA_DECODE 0

#include <stdlib.h>
#if TIME_THEORA_DECODE
#include <sys/time.h>
#endif
#include <time.h>
#include <string.h>

#define THEORA_VERSION(maj,min,rev) ((maj<<16)+(min<<8)+rev)

void
oggplay_init_theora(void *user_data) {

  OggPlayTheoraDecode   * decoder     = (OggPlayTheoraDecode *)user_data;

  if (decoder == NULL) {
    return;
  }
  
  theora_info_init(&(decoder->video_info));
  theora_comment_init(&(decoder->video_comment));
  decoder->granulepos_seen = 0;
  decoder->frame_delta = 0;
  decoder->y_width = 0;
  decoder->convert_to_rgb = 0;
  decoder->swap_rgb = 0;
  decoder->decoder.decoded_type = OGGPLAY_YUV_VIDEO;
}

void
oggplay_shutdown_theora(void *user_data) {
  OggPlayDecode         * common;
  OggPlayTheoraDecode   * decoder = (OggPlayTheoraDecode *)user_data;

  if (decoder == NULL) {
    return;
  }
  
  if ((common = &(decoder->decoder)) == NULL) {
    return;
  }
  
  if (common->initialised == 1 && decoder->decoder.num_header_packets == 0) {
    theora_clear(&(decoder->video_handle));
  }
  theora_info_clear(&(decoder->video_info));
  theora_comment_clear(&(decoder->video_comment));
}

int
oggplay_callback_theora (OGGZ * oggz, ogg_packet * op, long serialno,
                         void * user_data) {

  OggPlayTheoraDecode   * decoder     = (OggPlayTheoraDecode *)user_data;
  OggPlayDecode         * common      = NULL;
  ogg_int64_t             granulepos  = oggz_tell_granulepos(oggz);
  yuv_buffer              buffer;
  int                     granuleshift;
  long                    frame;
  OggPlayErrorCode        ret;
  
  /* check whether user_data is valid */
  if (decoder == NULL) {
    return OGGZ_STOP_ERR;
  }
  
  if ((common = &(decoder->decoder)) == NULL) {
    return OGGZ_STOP_ERR;
  }
    
#if TIME_THEORA_DECODE
  struct timeval          tv;
  struct timeval          tv2;
  int                     musec;
#endif

  if (!common->active) {
    /*
     * don't decode other packets
     */
    return OGGZ_CONTINUE;
  }
  
  if ((granulepos > 0) && (common->last_granulepos > granulepos)) {
    /* 
     * the granule position is not monotonically increasing,
     * something wrong with the page!
     * skipping this page..... 
     */
    return OGGZ_CONTINUE;
  }
  
  /*
   * always decode headers
   */
  if (theora_packet_isheader(op) &&
	  common->num_header_packets > 0 &&
	  common->initialised != -1)
  {
    if (theora_decode_header(&(decoder->video_info), &(decoder->video_comment), op) < 0) {
      common->initialised |= -1;
      return OGGZ_CONTINUE;
    }

    /*
     * initialise width/stride/height data (this is common to all frames).
     * Use the buffer stride for the width to avoid passing negative stride
     * issues on to the user.
     */

    if (--(common->num_header_packets) == 0) {
      decoder->y_width = decoder->y_stride = decoder->video_info.frame_width;
      decoder->y_height = decoder->video_info.frame_height;

      if (decoder->video_info.pixelformat == OC_PF_444) {
        decoder->uv_width = decoder->uv_stride = decoder->video_info.frame_width;
        decoder->uv_height = decoder->video_info.frame_height;
      } else if (decoder->video_info.pixelformat == OC_PF_422) {
        decoder->uv_width = decoder->uv_stride = decoder->video_info.frame_width / 2;
        decoder->uv_height = decoder->video_info.frame_height;
      } else if (decoder->video_info.pixelformat == OC_PF_420) {
        decoder->uv_width = decoder->uv_stride = decoder->video_info.frame_width / 2;
        decoder->uv_height = decoder->video_info.frame_height / 2;
      } else {
        common->initialised |= -1;
        return OGGZ_CONTINUE;
      }

     if (decoder->y_width == 0 ||
         decoder->y_height == 0 || 
         decoder->uv_width == 0 ||
         decoder->uv_height == 0) {
       /* it's a theora track with one of it's plane's dimension 0
        * decoding this track is not possible.
        */
       common->initialised |= -1;
       return OGGZ_CONTINUE;
     }

      /* Ensure the offsets do not push the viewable area outside of the decoded frame. */
      if 
      (
        ((decoder->video_info.height - decoder->video_info.offset_y)<decoder->video_info.frame_height)
        ||
        ((decoder->video_info.width - decoder->video_info.offset_x)<decoder->video_info.frame_width)
      )
      {
        common->initialised |= -1;
        return OGGZ_CONTINUE;
      }
      
      if (theora_decode_init(&(decoder->video_handle), &(decoder->video_info))) {
        common->initialised |= -1;
        return OGGZ_CONTINUE;
      }

      common->initialised |= 1;
    }
    return OGGZ_CONTINUE;
  } else if (common->num_header_packets != 0) {
    /*
     * Invalid Ogg file. Missing headers
     *
     */
    return -1;
  }
  
  /*
   * if we get to here then we've passed all the header packets
   */
  if (common->current_loc == -1)
    common->current_loc = 0;

  /*
   * Decode the frame
   */

#if TIME_THEORA_DECODE
  gettimeofday(&tv, NULL);
#endif

  if (theora_decode_packetin(&(decoder->video_handle), op) < 0) {
    return OGGZ_CONTINUE;
  }

  if (theora_decode_YUVout(&(decoder->video_handle), &buffer) < 0) {
    return OGGZ_CONTINUE;
  }

#if TIME_THEORA_DECODE
  gettimeofday(&tv2, NULL);
  musec = tv2.tv_usec - tv.tv_usec;
  if (tv2.tv_sec > tv.tv_sec)
    musec += (tv2.tv_sec - tv.tv_sec) * 1000000;
  printf("decode took %dus\n", musec);
#endif

  if (granulepos != -1) {
    int version = 
      THEORA_VERSION(decoder->video_info.version_major, 
                     decoder->video_info.version_minor,
                     decoder->video_info.version_subminor);

    /* 
     * save last granule position in order to be able to validate
     * that it's monotonically increasing
     */
    common->last_granulepos = granulepos;

    /* calculate the frame number */
    granuleshift = oggz_get_granuleshift(oggz, serialno);
    frame = (granulepos >> granuleshift);
    /* From theora bitstream version 3.2.1 onwards, frame granule numbers are
     * relative to the end of the frame, i.e. frame granule numbers start at 1.
     * We calcualte the presentation time as frame_number * granule_period,
     * but that's only correct if frame numbers start at 0 (else it's the end
     * time), so subtract 1 from the frame number if this is a theora stream
     * of version 3.2.1 or greater to ensure correct presentation time
     * calculation.
     */
    if (version >= THEORA_VERSION(3,2,1)) {
      frame--;
    }
    frame += (granulepos & ((1 << granuleshift) - 1));
    
    /* calculate the current location in the stream */
    common->current_loc = frame * common->granuleperiod;    
  } else {
    common->current_loc = -1;
  }

  if
  (
    (common->current_loc == -1)
    ||
    (common->current_loc >= common->player->presentation_time)
  )
  {
    /*
     * store the frame,
     * use the buffer stride for the width to avoid passing negative stride
     * issues on to the user.
     */
    ret = oggplay_data_handle_theora_frame(decoder, &buffer);
    if (ret != E_OGGPLAY_CONTINUE) {
      return OGGZ_ERR_OUT_OF_MEMORY;
    }
  }

  if (op->e_o_s) {
    common->active = 0;
    common->player->active_tracks--;
  }

  return OGGZ_CONTINUE;

}

void
oggplay_init_cmml (void * user_data) {

  OggPlayCmmlDecode * decoder = (OggPlayCmmlDecode *)user_data;
  
  if (decoder == NULL) {
    return;
  }
  
  decoder->decoder.decoded_type = OGGPLAY_CMML;
  decoder->granuleshift = 32; /* default */
}

int
oggplay_callback_cmml (OGGZ * oggz, ogg_packet * op, long serialno,
                       void * user_data) {

  OggPlayCmmlDecode * decoder     = (OggPlayCmmlDecode *)user_data;
  OggPlayDecode     * common      = NULL;
  ogg_int64_t         granulepos  = oggz_tell_granulepos (oggz);
  OggPlayErrorCode    ret;
  
  if (decoder == NULL) {
    return OGGZ_STOP_ERR;
  }
    
  if ((common = &(decoder->decoder)) == NULL) {
    return OGGZ_STOP_ERR;
  }

  if (common->num_header_packets) {
    /*
     * Process the headers of the CMML stream. 
     */
     
    if (common->num_header_packets == 3) {
      /* The CMML ident header packet */
      if (memcmp(op->packet, "CMML\0\0\0\0", 8) == 0) {
        decoder->granuleshift = op->packet[28];
      } else {
        /* Missing ident header ... */
        common->initialised |= -1;
      }
    } else if (common->num_header_packets == 2) {
      /* CMML secondary header, with xml preamble and cmml tag */
    } else if (common->num_header_packets == 1) {
      /* CMML secondary header, head tag */
    }
    
    if (!(--common->num_header_packets))
      common->initialised |= 1;

  } else {
    /*
     * Process the CMML stream content.
     */
      
    if (decoder->granuleshift > 0) {
      granulepos >>= decoder->granuleshift;
    }

    common->current_loc = granulepos * common->granuleperiod;

    ret = oggplay_data_handle_cmml_data (common, op->packet, op->bytes);
    if (ret != E_OGGPLAY_CONTINUE) {
      return OGGZ_ERR_OUT_OF_MEMORY;
    }
  }

  return OGGZ_CONTINUE;

}

void
oggplay_init_skel (void * user_data) {

  OggPlaySkeletonDecode * decoder = (OggPlaySkeletonDecode *)user_data;

  decoder->presentation_time = 0;
  decoder->base_time = 0;
}

static inline unsigned long extract_int32(unsigned char *data) {
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static inline ogg_int64_t extract_int64(unsigned char *data) {
  return ((ogg_int64_t)(extract_int32(data))) |
         (((ogg_int64_t)(extract_int32(data + 4))) << 32);
}

int
oggplay_callback_skel (OGGZ * oggz, ogg_packet * op, long serialno,
                       void * user_data) {

  OggPlaySkeletonDecode * decoder = (OggPlaySkeletonDecode *)user_data;
  
  /* check whether user_input is valid */
  if (decoder == NULL) {
    return OGGZ_STOP_ERR;
  }
  
  if (strncmp((char *)op->packet, "fishead", 7) == 0) {
    ogg_int64_t pt_num, pt_den, bt_num, bt_den;

    pt_num = extract_int64(op->packet + 12);
    pt_den = extract_int64(op->packet + 20);
    bt_num = extract_int64(op->packet + 28);
    bt_den = extract_int64(op->packet + 36);

    if (pt_den != 0) {
      decoder->presentation_time = OGGPLAY_TIME_INT_TO_FP(pt_num) / pt_den;
    } else {
      decoder->presentation_time = 0;
    }
    if (bt_den != 0) {
      decoder->base_time = OGGPLAY_TIME_INT_TO_FP(bt_num) / bt_den;
    } else {
      decoder->base_time = 0;
    }

    /* initialise the presentation times in the player to the values recorded in the skeleton */
    decoder->decoder.player->presentation_time = decoder->presentation_time;
    
    decoder->decoder.initialised = 1;
    decoder->decoder.num_header_packets--;
  } else {
    int i;
    long          preroll       = extract_int32(op->packet + 44);
    long          serialno      = extract_int32(op->packet + 12);
    OggPlay     * player        = decoder->decoder.player; 
    //ogg_int64_t   start_granule = extract_int64(op->packet + 36);

    for (i = 1; i < player->num_tracks; i++) {
      if (player->decode_data[i]->serialno == serialno) {
        player->decode_data[i]->preroll = preroll;
        break;
      }
    }
  }

  return OGGZ_CONTINUE;

}

int
oggplay_fish_sound_callback_floats(FishSound * fsound, float ** pcm, 
                                   long frames, void *user_data) {

  OggPlayAudioDecode  * decoder = (OggPlayAudioDecode *)user_data;
  OggPlayDecode       * common = NULL;  

  if (decoder == NULL) {
    return FISH_SOUND_STOP_ERR;
  }
  
  if ((common = &(decoder->decoder)) == NULL) {
    return FISH_SOUND_STOP_ERR;
  }

  /*
   * calculate the current location here so that it's only updated when
   * audio data is actually available for processing
   */
  if (common->last_granulepos > 0) {
    common->current_loc = common->last_granulepos * common->granuleperiod;
  } else {
    common->current_loc = -1;
  }

  if
  (
    (common->current_loc == -1)
    ||
    (common->current_loc >= common->player->presentation_time)
  )
  {
    /*
     * store the frame
     */
    oggplay_data_handle_audio_data(common, (short *)pcm, 
                                   frames, sizeof(float));

    return FISH_SOUND_STOP_ERR;
  }

  return FISH_SOUND_CONTINUE;
}

void
oggplay_init_audio (void * user_data) {

  OggPlayAudioDecode  * decoder = (OggPlayAudioDecode *)user_data;
  
  if (decoder == NULL) {
    return;
  }

  decoder->sound_handle = fish_sound_new(FISH_SOUND_DECODE,
                                         &(decoder->sound_info));

	if (decoder->sound_handle == NULL) {
    return;
	}
		
  decoder->sound_info.channels = 0;
  fish_sound_set_decoded_float_ilv(decoder->sound_handle,
                                   oggplay_fish_sound_callback_floats,
                                   (void *)decoder);

  decoder->decoder.decoded_type = OGGPLAY_FLOATS_AUDIO;
}

void
oggplay_shutdown_audio(void *user_data) {

  OggPlayAudioDecode   * decoder = (OggPlayAudioDecode *)user_data;
  
  if (decoder == NULL) {
    return;
  }
  
  fish_sound_delete(decoder->sound_handle);

}

int
oggplay_callback_audio (OGGZ * oggz, ogg_packet * op, long serialno,
                        void * user_data) {

  OggPlayAudioDecode   * decoder     = (OggPlayAudioDecode *)user_data;
  OggPlayDecode        * common      = NULL;
  ogg_int64_t            granulepos  = oggz_tell_granulepos(oggz);
  long                   bytes_read;

  /* check user input (user_data) */
  if (decoder == NULL) {
    return OGGZ_STOP_ERR;
  }
  
  if ((common = &(decoder->decoder)) == NULL) {
    return OGGZ_STOP_ERR;
  }
  
  if (granulepos > 0 && (!common->active)) {
    return OGGZ_CONTINUE;
  }
  
  if ((granulepos > 0) && (common->last_granulepos > granulepos)) {
    return OGGZ_CONTINUE;
  }
  
  /* Blindly register that we've processed a header packet. */
  if (common->num_header_packets) --common->num_header_packets;
  
  common->last_granulepos = granulepos;

  fish_sound_prepare_truncation (decoder->sound_handle, op->granulepos, 
                                 op->e_o_s);
  
  bytes_read = fish_sound_decode (decoder->sound_handle, op->packet, op->bytes);
  switch (bytes_read) {
    case FISH_SOUND_ERR_OUT_OF_MEMORY:
      /* we ran out of memory... stop decoding. */
      return OGGZ_ERR_OUT_OF_MEMORY;
      
    case FISH_SOUND_ERR_GENERIC:
    {
      /* 
       * error occured while decoding the audio track 
       * disable the track, but if there are other tracks to decode
       * contine decoding...
       */
      
      common->active = 0;
      
      if (common->player->active_tracks) common->player->active_tracks--;
      if (common->num_header_packets >= 0) common->initialised |= -1;

      return OGGZ_CONTINUE;
    }
    
    default:
      /* there was no problem with decoding */
      if (!common->num_header_packets) common->initialised |= 1;
      break;
  }

  if (decoder->sound_info.channels == 0) {
    fish_sound_command(decoder->sound_handle, FISH_SOUND_GET_INFO,
                       &(decoder->sound_info), sizeof(FishSoundInfo));
  }

  if (op->e_o_s) {
    common->active = 0;
    common->player->active_tracks--;
  }

  return OGGZ_CONTINUE;
}

void
oggplay_init_kate(void *user_data) {

#ifdef HAVE_KATE
  int ret;
  OggPlayKateDecode   * decoder     = (OggPlayKateDecode *)user_data;

  if (decoder == NULL) {
    return;
  }
  
  decoder->decoder.decoded_type = OGGPLAY_KATE;
  kate_info_init (&(decoder->k_info));
  kate_comment_init (&(decoder->k_comment));

#ifdef HAVE_TIGER
  decoder->use_tiger = 1;
  decoder->overlay_dest = -1;
  decoder->swap_rgb = 0;
  decoder->default_width = -1;
  decoder->default_height = -1;

  ret = tiger_renderer_create(&(decoder->tr));
  if (ret < 0) {
    /* what to do ? */
    decoder->tr = NULL;
  }
  if (decoder->use_tiger) {
    decoder->decoder.decoded_type = OGGPLAY_RGBA_VIDEO;
  }
#endif

#endif
}

void
oggplay_shutdown_kate(void *user_data) {

#ifdef HAVE_KATE
  OggPlayKateDecode   * decoder = (OggPlayKateDecode *)user_data;
  
  if (decoder == NULL) {
    return;
  }
#ifdef HAVE_TIGER
  if (decoder->tr) {
    tiger_renderer_destroy(decoder->tr);
  }
#endif

  if (decoder->decoder.initialised == 1) {
    kate_clear (&(decoder->k_state));
  }
  
  kate_info_clear (&(decoder->k_info));
  kate_comment_clear (&(decoder->k_comment));
  
#endif
}

int
oggplay_callback_kate (OGGZ * oggz, ogg_packet * op, long serialno,
                       void * user_data) {

#ifdef HAVE_KATE
  OggPlayKateDecode     * decoder     = (OggPlayKateDecode *)user_data;
  OggPlayDecode         * common      = NULL;
  ogg_int64_t             granulepos  = oggz_tell_granulepos(oggz);
  int                     granuleshift;
  ogg_int64_t             base, offset;
  kate_packet kp;
  const kate_event *ev = NULL;
  int ret;

  /*
   * Basic error checking.
   */
  if (decoder == NULL) {
    return OGGZ_STOP_ERR;
  }
  
  if ((common = &(decoder->decoder)) == NULL) {
    return OGGZ_STOP_ERR;
  } 

  /*
   * Stop processing the Ogg packet if the stream is not active.
   */
  if (!common->active) {
    return OGGZ_CONTINUE;
  }
  
  /* create a kate_packet from the received ogg_packet */
  kate_packet_wrap (&kp, op->bytes, op->packet);

  /* 
   * Decode the headers of the kate stream.
   */
  if (common->num_header_packets) {
    ret = kate_decode_headerin (&(decoder->k_info), &(decoder->k_comment), &kp);

    if (ret == KATE_E_OUT_OF_MEMORY) {
      return OGGZ_ERR_OUT_OF_MEMORY;
    }
    
    common->initialised |= (ret < 0 ? -1 : ret);
    common->num_header_packets--;
        
    /* if we _successfully_ processed all the headers initialise the decoder */
    if (!common->num_header_packets && (common->initialised == 1)) {
      ret = kate_decode_init (&(decoder->k_state), &(decoder->k_info));
      
      if (ret == KATE_E_OUT_OF_MEMORY) {
        return OGGZ_ERR_OUT_OF_MEMORY;
      } else if (ret < 0) {
        common->initialised |= -1;
      }
    }
    
    return OGGZ_CONTINUE;
  } 
    
  /*
   * Decode the payload of the stream.
   */
   
  ret = kate_decode_packetin (&(decoder->k_state), &kp);
  if (ret == KATE_E_OUT_OF_MEMORY) {
    return OGGZ_ERR_OUT_OF_MEMORY;
  } else if (ret < 0){
    return OGGZ_CONTINUE;
  }
  
  ret = kate_decode_eventout (&(decoder->k_state), &ev);
  if (ret < 0) {
    return OGGZ_CONTINUE;
  }
  
  if (granulepos != -1) {
    granuleshift = oggz_get_granuleshift(oggz, serialno);
    base = (granulepos >> granuleshift);
    offset = granulepos - (base << granuleshift);
    common->current_loc = (base+offset) * common->granuleperiod;
  } else {
    common->current_loc = -1;
  }

  if
  (
    (common->current_loc == -1)
    ||
    (common->current_loc >= common->player->presentation_time)
  )
  {
    /*
     * process the data from the packet
     * */
    if (ev) {
      if (oggplay_data_handle_kate_data(decoder, ev) != E_OGGPLAY_CONTINUE) {
        return OGGZ_ERR_OUT_OF_MEMORY;
      }
    }
  }

  if (op->e_o_s) {
    common->active = 0;
  }

#endif

  return OGGZ_CONTINUE;

}

OggPlayCallbackFunctions callbacks[] = {
  {oggplay_init_theora, oggplay_callback_theora, oggplay_shutdown_theora,
        sizeof(OggPlayTheoraDecode)},        /* THEORA */
  {oggplay_init_audio, oggplay_callback_audio, oggplay_shutdown_audio,
        sizeof(OggPlayAudioDecode)},         /* VORBIS */
  {oggplay_init_audio, oggplay_callback_audio, oggplay_shutdown_audio,
        sizeof(OggPlayAudioDecode)},         /* SPEEX */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}, /* PCM */
  {oggplay_init_cmml, oggplay_callback_cmml, NULL, sizeof(OggPlayCmmlDecode)}, /* CMML */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}, /* ANX2 */
  {oggplay_init_skel, oggplay_callback_skel, NULL,
        sizeof(OggPlaySkeletonDecode)},      /* SKELETON */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}, /* FLAC0 */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}, /* FLAC */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}, /* ANXDATA */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}, /* CELT */
  {oggplay_init_kate, oggplay_callback_kate, oggplay_shutdown_kate,
        sizeof(OggPlayKateDecode)},          /* KATE */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}, /* DIRAC */
  {NULL, NULL, NULL, sizeof(OggPlayDecode)}  /* UNKNOWN */
};

OggPlayDecode *
oggplay_initialise_decoder(OggPlay *me, int content_type, long serialno) {

  ogg_int64_t    num;
  ogg_int64_t    denom;
  OggPlayDecode *decoder = NULL;

  if (me == NULL)
    return NULL;

  decoder = oggplay_malloc (callbacks[content_type].size);

  if (decoder == NULL)
    return NULL;

  decoder->serialno = serialno;
  decoder->content_type = content_type;
  decoder->content_type_name =
          oggz_stream_get_content_type (me->oggz, serialno);
  decoder->active = 1;
  decoder->final_granulepos = -1;
  decoder->player = me;
  decoder->decoded_type = OGGPLAY_TYPE_UNKNOWN;
  decoder->num_header_packets = 
          oggz_stream_get_numheaders (me->oggz, serialno);

  /*
   * set the StreamInfo to unitialised until we get some real data in
   */
  decoder->stream_info = OGGPLAY_STREAM_UNINITIALISED;

  /*
   * set to -1 until headers decoded
   */
  decoder->current_loc = -1;
  decoder->last_granulepos = 0;

  /*
   * the offset is how far advanced or delayed this track is to the "standard"
   * time position.  An offset of 1000, for example, indicates that data for
   * this track arrives 1 second in advance of data for other tracks
   */
  decoder->offset = 0;

  oggz_get_granulerate(me->oggz, serialno, &num, &denom);

  /*
   * convert num and denom to a 32.32 fixed point value
   */
  if (num != 0) {
    decoder->granuleperiod = OGGPLAY_TIME_INT_TO_FP(denom) / num;
  } else {
    decoder->granuleperiod = 0;
  }
  
  if (callbacks[content_type].init != NULL) {
    callbacks[content_type].init(decoder);
    decoder->initialised = 0;
  } else {
    decoder->initialised = -1;
  }

  oggplay_data_initialise_list(decoder);

  return decoder;
}


/*
 * this function needs to be called on each track to clear up allocated memory
 */
void
oggplay_callback_shutdown(OggPlayDecode *decoder) {
  
  if (decoder == NULL) {
    return;
  }
  
  if (callbacks[decoder->content_type].shutdown != NULL) {
    callbacks[decoder->content_type].shutdown(decoder);
  }

  oggplay_data_shutdown_list(decoder);

  oggplay_free(decoder);
}


/*
 * this is the callback that is used before all track types have been
 * determined - i.e. at the beginning of an ogg bitstream or at the start
 * of a new chain
 */
int
oggplay_callback_predetected (OGGZ *oggz, ogg_packet *op, long serialno,
                              void *user_data) {

  OggPlay     * me            = (OggPlay *)user_data;
  int           i;
  int           content_type  = 0;
  int           ret           = OGGZ_CONTINUE;
  short         new_stream    = 1;
  short         read_more     = 0;
  ogg_int64_t   granulepos    = oggz_tell_granulepos(oggz);
  
  if (me == NULL) {
    return OGGZ_STOP_ERR;
  }
  
  content_type = oggz_stream_get_content (me->oggz, serialno);
  
  for (i = 0; i < me->num_tracks; i++) {
    if (serialno == me->decode_data[i]->serialno) {
      /*
       * call appropriate callback
       */
      if (callbacks[content_type].callback != NULL) {
        ret = callbacks[content_type].callback(oggz, op, serialno,
                                               me->decode_data[i]);
      }
      
      new_stream = 0;
    }
    
    /* a track that hasn't got all it's headers, but others are already having
     * valueable data. A badly multiplexed Ogg file, as the specification
     * requires that all headers has to be before any data comes.
     * Instead of marking the whole file as a bad one, try to decode
     * the streams, which headers are successfully decoded.
     * and disable the tracks that haven't got enough header info.
     */
    if (granulepos && me->decode_data[i]->num_header_packets) {
      me->decode_data[i]->initialised = -1;
    }
    
    /* 
     * check whether there is a stream that has not
     * decoded all it's headers
     */
    read_more |= (me->decode_data[i]->num_header_packets && (me->decode_data[i]->initialised != -1));
  }

  if (new_stream) {
    /* check for possible overflow ... */
    if 
    (
      (++me->num_tracks <= 0)
      ||
      (OGGPLAY_TYPE_MAX(size_t)/(me->num_tracks) < sizeof(OggPlayCallbackInfo))
      ||
      (OGGPLAY_TYPE_MAX(size_t)/me->num_tracks < sizeof(long))
    ) 
    {
      return OGGZ_STOP_ERR;
    }
  
    me->callback_info = oggplay_realloc (me->callback_info,
                    sizeof (OggPlayCallbackInfo) * me->num_tracks);
    if (me->callback_info == NULL)
      return OGGZ_ERR_OUT_OF_MEMORY;

    me->decode_data = oggplay_realloc (me->decode_data, 
                                       sizeof (long) * me->num_tracks);
    if (me->decode_data == NULL)
      return OGGZ_ERR_OUT_OF_MEMORY;

    me->decode_data[me->num_tracks - 1] = 
        oggplay_initialise_decoder(me, content_type, serialno);
    if (me->decode_data[me->num_tracks - 1] == NULL)
      return OGGZ_ERR_OUT_OF_MEMORY; 

    /*
     * call appropriate callback
     */
    if (callbacks[content_type].callback != NULL) {
      ret = callbacks[content_type].callback(oggz, op, serialno,
                                             me->decode_data[me->num_tracks - 1]);
    }
  } else if (!read_more) {
    /* 
     * all tracks' headers has been processed 
     * initialisation phase done, process the payloads.
     */
    me->all_tracks_initialised = 1;
    
    /* set up all the callbacks for the detected streams */
    for (i = 0; i < me->num_tracks; i++) {
      serialno = me->decode_data[i]->serialno;
      content_type = oggz_stream_get_content (me->oggz, serialno);
      if (oggz_set_read_callback (me->oggz, serialno, 
                                  callbacks[content_type].callback, 
                                  me->decode_data[i]) != 0)
      {
        return OGGZ_STOP_ERR;
      }
    }
    
    /* disable the callback for unforeseen streams */
    oggz_set_read_callback (me->oggz, -1, NULL, NULL);
  }
  
  /* read the header part of the ogg content in a packet-by-packet manner */
  return ((ret < 0) ? ret : OGGZ_STOP_OK);
}
