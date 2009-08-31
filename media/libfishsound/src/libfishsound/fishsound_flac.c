/*
   Copyright (C) 2007 Annodex Association

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Annodex Association nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ASSOCIATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Original patches by Tobias Gehrig, 2005
 * http://www.annodex.net/software/libfishsound/libfishsound-flac/
 *
 * The Ogg FLAC mapping is documented in:
 * http://flac.sourceforge.net/ogg_mapping.html
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "private.h"
#include "convert.h"

#define DEBUG_VERBOSE 2
/* #define DEBUG */
/* #define DEBUG_LEVEL DEBUG_VERBOSE */
#include "debug.h"

#if HAVE_FLAC

#include "FLAC/all.h"

#define BITS_PER_SAMPLE 24

typedef struct _FishSoundFlacInfo {
  FLAC__StreamDecoder *fsd;
  FLAC__StreamEncoder *fse;
  unsigned char * buffer;
  char header;
  long bufferlength;
  unsigned long packetno;
  struct {
    unsigned char major, minor;
  } version;
  unsigned short header_packets;
  void * ipcm;
#if FS_DECODE
  float * pcm_out[8]; /* non-interleaved pcm, output (decode only);
                       * FLAC does max 8 channels */
#endif
#if FS_ENCODE
  FLAC__StreamMetadata * enc_vc_metadata; /* FLAC metadata structure for
                                           * vorbiscomments (encode only) */
#endif
} FishSoundFlacInfo;

int
fish_sound_flac_identify (unsigned char * buf, long bytes)
{
  if (bytes < 8) return FISH_SOUND_UNKNOWN;
  if (buf[0] != 0x7f) return FISH_SOUND_UNKNOWN;
  if (!strncmp ((char *)buf+1, "FLAC", 4)) {
    debug_printf(1, "flac found");
    /* if only a short buffer was passed, do a weak identify */
    if (bytes == 8) return FISH_SOUND_FLAC;

    /* otherwise, look for the fLaC header preceding STREAMINFO */
    if (!strncmp ((char *)buf+9, "fLaC", 4)) {
      return FISH_SOUND_FLAC;
    }
  }

  return FISH_SOUND_UNKNOWN;
}

static int
fs_flac_command (FishSound * fsound, int command, void * data, int datasize)
{
  return 0;
}

#if FS_DECODE
static FLAC__StreamDecoderReadStatus
fs_flac_read_callback(const FLAC__StreamDecoder *decoder,
                      FLAC__byte buffer[], unsigned *bytes,
                      void *client_data)
{
  FishSound* fsound = (FishSound*)client_data;
  FishSoundFlacInfo* fi = (FishSoundFlacInfo *)fsound->codec_data;
  debug_printf(DEBUG_VERBOSE, "fs_flac_read_callback: IN");
  if (fi->bufferlength > *bytes) {
    debug_printf(1, "too much data");
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  } else if (fi->bufferlength < 1) {
    debug_printf(1, "no data, %ld",fi->bufferlength);
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }

  memcpy(buffer, fi->buffer, fi->bufferlength);
  *bytes = fi->bufferlength;
  fi->bufferlength = 0;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus
fs_flac_write_callback(const FLAC__StreamDecoder *decoder,
                       const FLAC__Frame *frame,
                       const FLAC__int32 * const buffer[],
                       void *client_data)
{
  FishSound* fsound = (FishSound*)client_data;
  FishSoundFlacInfo* fi = (FishSoundFlacInfo *)fsound->codec_data;
  int i, j, channels, blocksize, offset;
  float * ipcm;

  channels = frame->header.channels;
  blocksize = frame->header.blocksize;

  debug_printf(DEBUG_VERBOSE, "IN, blocksize %d", blocksize);

  fsound->frameno += blocksize;

  if (fsound->callback.decoded_float) {
    float norm = 1.0 / ((1 << (frame->header.bits_per_sample - 1)));

    if (fsound->interleave) {
	FishSoundDecoded_FloatIlv dfi;
	float* retpcm;

        if ((ipcm = realloc(fi->ipcm, sizeof(float) * channels * blocksize)) == NULL)
          return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	fi->ipcm = ipcm;
	retpcm = (float*) fi->ipcm;
	for (i = 0; i < blocksize; i++) {
	  offset = i * channels;
	  for (j = 0; j < channels; j++)
	    retpcm[offset + j] = buffer[j][i] * norm;
	}
	dfi = (FishSoundDecoded_FloatIlv)fsound->callback.decoded_float_ilv;
	dfi (fsound, (float **)retpcm, blocksize, fsound->user_data);
      } else {
	FishSoundDecoded_Float df;
        FLAC__int32 * s = (FLAC__int32 *)buffer; /* de-interleave source */
	float *d; /* de-interleave dest */

        for (j = 0; j < channels; j++) {
          if ((ipcm = realloc(fi->pcm_out[j], sizeof(float) * blocksize)) == NULL)
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	  fi->pcm_out[j] = ipcm;
        }
	for (i = 0; i < blocksize; i++)
	  for (j = 0; j < channels; j++) {
	    d = fi->pcm_out[j];
	    d[i] = s[i*channels + j] * norm;
	  }
      	df = (FishSoundDecoded_Float)fsound->callback.decoded_float;
	df (fsound, fi->pcm_out, blocksize, fsound->user_data);
    }
  }
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
fs_flac_meta_callback(const FLAC__StreamDecoder *decoder,
                      const FLAC__StreamMetadata *metadata,
                      void *client_data)
{
  FishSound* fsound = (FishSound*)client_data;
  /*  FishSoundFlacInfo* fi = (FishSoundFlacInfo *)fsound->codec_data; */
  debug_printf(1, "IN");

  switch (metadata->type) {
  case FLAC__METADATA_TYPE_STREAMINFO:
    debug_printf(1, "channels %d, samplerate %d",
           metadata->data.stream_info.channels,
           metadata->data.stream_info.sample_rate);
    fsound->info.channels = metadata->data.stream_info.channels;
    fsound->info.samplerate = metadata->data.stream_info.sample_rate;
    break;
  default:
    debug_printf(1, "not yet implemented type");
    break;
  }
}

static void
fs_flac_error_callback(const FLAC__StreamDecoder *decoder,
                       FLAC__StreamDecoderErrorStatus status,
                       void *client_data)
{
  debug_printf(1, "IN");
  fprintf(stderr, "FLAC ERROR: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}
#endif
#if FS_DECODE
static void*
fs_flac_decode_header (FishSound * fsound, unsigned char *buf, long bytes)
{
  FishSoundFlacInfo *fi = fsound->codec_data;

  if (bytes < 9) return NULL;
  if (buf[0] != 0x7f) return NULL;
  if (strncmp((char *)buf+1, "FLAC", 4) != 0) return NULL;
  fi->version.major = buf[5];
  fi->version.minor = buf[6];
  debug_printf(1, "Flac Ogg Mapping Version: %d.%d",
         fi->version.major, fi->version.minor);
  fi->header_packets = buf[7] << 8 | buf[8];
  debug_printf(1, "Number of Header packets: %d", fi->header_packets);

  if ((fi->fsd = FLAC__stream_decoder_new()) == NULL) {
    debug_printf (1, "unable to create new stream_decoder");
    return NULL;
  }

#if defined (HAVE_FLAC_1_1_2)
  FLAC__stream_decoder_set_read_callback(fi->fsd, fs_flac_read_callback);
  FLAC__stream_decoder_set_write_callback(fi->fsd, fs_flac_write_callback);
  FLAC__stream_decoder_set_metadata_callback(fi->fsd, fs_flac_meta_callback);
  FLAC__stream_decoder_set_error_callback(fi->fsd, fs_flac_error_callback);
  FLAC__stream_decoder_set_client_data(fi->fsd, fsound);

  if (FLAC__stream_decoder_init(fi->fsd) != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
    return NULL;
#elif defined (HAVE_FLAC_1_1_3)
  if (FLAC__stream_decoder_init_stream
      (fi->fsd,
       fs_flac_read_callback,
       NULL, /* seek callback */
       NULL, /* tell callback */
       NULL, /* length callback */
       NULL, /* EOF callback */
       fs_flac_write_callback,
       fs_flac_meta_callback,
       fs_flac_error_callback,
       fsound
       ) != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
     return NULL;
#endif

  return fi->fsd;
}

static long
fs_flac_decode (FishSound * fsound, unsigned char * buf, long bytes)
{
  FishSoundFlacInfo *fi = fsound->codec_data;

  debug_printf(DEBUG_VERBOSE, "IN, fi->packetno = %ld", fi->packetno);

  if (fi->packetno == 0) {
    if (fs_flac_decode_header (fsound, buf, bytes) == NULL) {
      debug_printf(1, "Error reading header");
      return -1;
    }
    if ((fi->buffer = fs_malloc(sizeof(unsigned char)*bytes)) == NULL)
      return FISH_SOUND_ERR_OUT_OF_MEMORY;

    memcpy(fi->buffer, buf+9, bytes-9);
    fi->bufferlength = bytes-9;
  }
  else if (fi->packetno <= fi->header_packets){
    unsigned char* tmp;
    debug_printf(1, "handling header (fi->header_packets = %d)",
                 fi->header_packets);

#if 0
    if (fi->packetno ==  1) fish_sound_comments_decode (fsound, buf, bytes);
#endif

    if ((buf[0] & 0x7) == 4) {
      int len = (buf[1]<<16) + (buf[2]<<8) + buf[3];
      debug_printf (1, "got vorbiscomments len %d", len);

      if (fish_sound_comments_decode (fsound, buf+4, len) == FISH_SOUND_ERR_OUT_OF_MEMORY) {
        fi->packetno++;
        return FISH_SOUND_ERR_OUT_OF_MEMORY;
      }
    }

    if ((tmp = fs_malloc(sizeof(unsigned char)*(fi->bufferlength+bytes))) == NULL)
      return FISH_SOUND_ERR_OUT_OF_MEMORY;

    memcpy(tmp, fi->buffer, fi->bufferlength);
    memcpy(tmp+fi->bufferlength, buf, bytes);
    fi->bufferlength += bytes;
    fs_free(fi->buffer);
    fi->buffer = tmp;
    if (fi->packetno == fi->header_packets) {
      if (FLAC__stream_decoder_process_until_end_of_metadata(fi->fsd) == false) {
        goto dec_err;
      }
      fs_free(fi->buffer);
    }
  } else {
    fi->buffer = buf;
    fi->bufferlength = bytes;
    if (FLAC__stream_decoder_process_single(fi->fsd) == false) {
      goto dec_err;
    }
  }
  fi->packetno++;

  return 0;

dec_err:
    switch (FLAC__stream_decoder_get_state(fi->fsd)) {
    case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
      return FISH_SOUND_ERR_OUT_OF_MEMORY;
    default:
      return FISH_SOUND_ERR_GENERIC;
    }
}
#else /* !FS_DECODE */

#define fs_flac_decode NULL

#endif


#if FS_ENCODE
static FLAC__StreamEncoderWriteStatus
fs_flac_enc_write_callback(const FLAC__StreamEncoder *encoder,
                           const FLAC__byte buffer[], unsigned bytes,
                           unsigned samples, unsigned current_frame,
                           void *client_data)
{
  FishSound* fsound = (FishSound*)client_data;
  FishSoundFlacInfo *fi = fsound->codec_data;

  debug_printf(1, "IN");
  debug_printf(1, "bytes: %d, samples: %d", bytes, samples);

  if (fsound->callback.encoded) {
    FishSoundEncoded encoded = (FishSoundEncoded) fsound->callback.encoded;
    if (fi->packetno == 0 && fi->header <= 1) {
      if (fi->header == 0) {
        /* libFLAC has called us with data containing the normal fLaC header
         * and a STREAMINFO block. Prepend the FLAC Ogg mapping header,
         * as described in http://flac.sourceforge.net/ogg_mapping.html.
         */
        debug_printf(1, "generating FLAC header packet: %c%c%c%c",
                     buffer[0], buffer[1], buffer[2], buffer[3]);

	if ((fi->buffer = (unsigned char*)fs_malloc(sizeof(unsigned char)*(bytes+9))) == NULL)
          return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

	fi->buffer[0] = 0x7f;
	fi->buffer[1] = 0x46; /* 'F' */
	fi->buffer[2] = 0x4c; /* 'L' */
	fi->buffer[3] = 0x41; /* 'A' */
	fi->buffer[4] = 0x43; /* 'C' */
	fi->buffer[5] = 1;    /* Version major generated by this file */
	fi->buffer[6] = 0;    /* Version minor generated by this file */
	fi->buffer[7] = 0;    /* MSB(be): Nr. other non-audio header packets */
	fi->buffer[8] = 1;    /* LSB(be): Nr. other non-audio header packets */
	memcpy (fi->buffer+9, buffer, bytes); /* fLaC header ++ STREAMINFO */
        fi->bufferlength = bytes+9;

	fi->header++;
      } else {
        /* Make a temporary copy of the metadata header to pass to the user
         * callback.
         */
	unsigned char* tmp;

        if ((tmp = (unsigned char*)fs_malloc(sizeof(unsigned char)*(bytes+fi->bufferlength))) == NULL)
          return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

	memcpy (tmp, fi->buffer, fi->bufferlength);
	memcpy (tmp+fi->bufferlength, buffer, bytes);
	fs_free(fi->buffer);
	fi->buffer = tmp;
	fi->bufferlength += bytes;
	fi->header++;
	encoded (fsound, (unsigned char *)fi->buffer, (long)fi->bufferlength,
		 fsound->user_data);
      }
    } else {
      fsound->frameno += samples;
      encoded (fsound, (unsigned char *)buffer, (long)bytes,
	       fsound->user_data);
    }
  }

  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static void
fs_flac_enc_meta_callback(const FLAC__StreamEncoder *encoder,
                          const FLAC__StreamMetadata *metadata,
                          void *client_data)
{
  /* FishSound* fsound = (FishSound*)client_data; */
  /* FishSoundFlacInfo* fi = (FishSoundFlacInfo *)fsound->codec_data; */
  debug_printf(1, "IN");

  switch (metadata->type) {
  case FLAC__METADATA_TYPE_STREAMINFO:
    debug_printf(1, "channels %d, samplerate %d",
           metadata->data.stream_info.channels,
           metadata->data.stream_info.sample_rate);
    /*
    fsound->info.channels = metadata->data.stream_info.channels;
    fsound->info.samplerate = metadata->data.stream_info.sample_rate;
    */
    break;
  default:
    debug_printf(1, "metadata type not yet implemented");
    break;
  }

  return;
}

/* Create a local alias for an unwieldy type name */
typedef FLAC__StreamMetadata_VorbisComment_Entry FLAC__VCEntry;

static void
fs_flac_metadata_free (FLAC__StreamMetadata * metadata)
{
  unsigned int i, length;
  FLAC__VCEntry * comments;

  if (metadata == NULL) return;

  length = metadata->data.vorbis_comment.num_comments;
  comments = metadata->data.vorbis_comment.comments;

  for (i = 0; i < length; i++) {
    fs_free (comments[i].entry);
  }

  fs_free (comments);
  fs_free (metadata);

  return;
}

static FLAC__byte *
fs_flac_encode_vcentry (const FishSoundComment * comment)
{
  FLAC__byte * entry;
  FLAC__uint32 length;
  size_t name_len=0, value_len=0;

  name_len = strlen(comment->name);
  length = name_len + 1;

  if (comment->value) {
    value_len = strlen (comment->value);
    length += value_len + 1;
  }

  if ((entry = fs_malloc (length)) == NULL)
    return NULL;

  /* We assume that comment->name, value are NUL terminated, as they were
   * produced by our own comments.c */
  strcpy ((char *)entry, comment->name);

  if (comment->value) {
    entry[name_len] = '=';
    strcpy ((char *)&entry[name_len+1], comment->value);
  }

  entry[length-1] = '\0';

  return entry;
}

static FLAC__StreamMetadata *
fs_flac_encode_vorbiscomments (FishSound * fsound)
{
  FishSoundFlacInfo * fi = fsound->codec_data;
  FLAC__StreamMetadata * metadata = NULL;
  const FishSoundComment * comment;
  unsigned int i=0, length=0, total_length;
  FLAC__VCEntry * comments;

  /* libFLAC seems to require us to know the total length of the generated
   * vorbiscomment packet, even though it will silently generate the
   * vendor string. Hence, this value was determined by inspection for
   * the version "reference libFLAC 1.1.2"
   */
  total_length = 40;

  /* Count the number of comments */
  for (comment = fish_sound_comment_first (fsound); comment;
       comment = fish_sound_comment_next (fsound, comment)) {
    length++;
  }

  if (length == 0) return NULL;

  if ((comments = (FLAC__VCEntry *)fs_malloc (sizeof(FLAC__VCEntry) * length)) == NULL)
    goto encode_vc_oom;
  
  for (comment = fish_sound_comment_first (fsound); comment;
       comment = fish_sound_comment_next (fsound, comment)) {
    if ((comments[i].entry = fs_flac_encode_vcentry (comment)) == NULL) {
    }
    comments[i].length = strlen((char *)comments[i].entry);

    /* In the generated vorbiscomment data, each entry is preceded by a
     * 32bit length specifier. */
    total_length += 4 + comments[i].length;
    i++;
  }

  if ((metadata = (FLAC__StreamMetadata *) fs_malloc (sizeof (*metadata))) == NULL)
    goto encode_vc_oom;

  metadata->type = FLAC__METADATA_TYPE_VORBIS_COMMENT;
  metadata->is_last = true;
  metadata->length = total_length;
  /* Don't bother setting the vendor_string, as libFLAC ignores it */
  metadata->data.vorbis_comment.num_comments = length;
  metadata->data.vorbis_comment.comments = comments;

  /* Remember the allocated metadata */
  fi->enc_vc_metadata = metadata;

  return metadata;

encode_vc_oom:
  if (metadata != NULL)
    fs_free (metadata);

  /* Unwind allocated comment entries */
  for (i--; i >= 0; i--) {
    if (comments[i].entry != NULL)
      fs_free (comments[i].entry);
  }

  if (comments != NULL)
    fs_free (comments);

  return NULL;
}

static FishSound *
fs_flac_enc_headers (FishSound * fsound)
{
  FishSoundFlacInfo * fi = fsound->codec_data;
  FLAC__StreamMetadata * metadata;

  fi->fse = FLAC__stream_encoder_new();
  FLAC__stream_encoder_set_channels(fi->fse, fsound->info.channels);
  FLAC__stream_encoder_set_sample_rate(fi->fse, fsound->info.samplerate);
  FLAC__stream_encoder_set_bits_per_sample(fi->fse, BITS_PER_SAMPLE);

#if defined (HAVE_FLAC_1_1_2)
  FLAC__stream_encoder_set_write_callback(fi->fse, fs_flac_enc_write_callback);
  FLAC__stream_encoder_set_metadata_callback(fi->fse, fs_flac_enc_meta_callback);
  FLAC__stream_encoder_set_client_data(fi->fse, fsound);
#endif

  metadata = fs_flac_encode_vorbiscomments (fsound);
  if (metadata != NULL)
    FLAC__stream_encoder_set_metadata (fi->fse, &metadata, 1);

  /* FLAC__stream_encoder_set_total_samples_estimate(fi->fse, ...);*/

#if defined (HAVE_FLAC_1_1_2)
  if (FLAC__stream_encoder_init(fi->fse) != FLAC__STREAM_ENCODER_OK)
    return NULL;
#elif defined (HAVE_FLAC_1_1_3)
  if (FLAC__stream_encoder_init_stream
      (fi->fse,
       fs_flac_enc_write_callback,
       NULL, /* seek callback */
       NULL, /* tell callback */
       fs_flac_enc_meta_callback,
       fsound
       ) != FLAC__STREAM_ENCODER_OK)
     return NULL;

#endif

  return fsound;
}

static long
fs_flac_encode_fatal (FishSoundFlacInfo *fi, long err)
{
  FLAC__stream_encoder_delete (fi->fse);
  fi->fse = NULL;
  return err;
}

static long
fs_flac_encode_f (FishSound * fsound, float * pcm[], long frames)
{
  FishSoundFlacInfo *fi = fsound->codec_data;
  FLAC__int32 *buffer, *ipcm;
  float * p, norm = (1 << (BITS_PER_SAMPLE - 1));
  long i;
  int j, channels = fsound->info.channels;

  debug_printf("IN, frames = %ld", frames);

  if ((ipcm = realloc(fi->ipcm, sizeof(FLAC__int32) * channels * frames)) == NULL)
    return FISH_SOUND_ERR_OUT_OF_MEMORY;

  fi->ipcm = ipcm;
  buffer = (FLAC__int32*) fi->ipcm;
  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      p = pcm[j];
      buffer[i*channels + j] = (FLAC__int32) (p[i] * norm);
    }
  }

  if (fi->packetno == 0)
    fs_flac_enc_headers (fsound);

  /* We could have used FLAC__stream_encoder_process() and a more direct
   * conversion loop above, rather than converting and interleaving. */
  if (FLAC__stream_encoder_process_interleaved(fi->fse, buffer, frames) == false) {
    switch (FLAC__stream_encoder_get_state (fi->fse)) {
    case FLAC__STREAM_ENCODER_OK:
    case FLAC__STREAM_ENCODER_UNINITIALIZED:
      break;
    case FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR:
      return fs_flac_encode_fatal (fi, FISH_SOUND_ERR_OUT_OF_MEMORY);
    default:
      return fs_flac_encode_fatal (fi, FISH_SOUND_ERR_GENERIC);
    }
  }

  fi->packetno++;

  return frames;
}

static long
fs_flac_encode_f_ilv (FishSound * fsound, float ** pcm, long frames)
{
  FishSoundFlacInfo *fi = fsound->codec_data;
  FLAC__int32 *buffer, *ipcm;
  float * p = (float*)pcm, norm = (1 << (BITS_PER_SAMPLE - 1));
  long i, length = frames * fsound->info.channels;

  debug_printf(1, "IN, frames = %ld", frames);

  if ((ipcm = realloc(fi->ipcm, sizeof(FLAC__int32)*fsound->info.channels*frames)) == NULL)
    return FISH_SOUND_ERR_OUT_OF_MEMORY;

  fi->ipcm = ipcm;
  buffer = (FLAC__int32*) fi->ipcm;
  for (i=0; i<length; i++)
    buffer[i] = p[i] * norm;

  if (fi->packetno == 0)
    fs_flac_enc_headers (fsound);

  if (FLAC__stream_encoder_process_interleaved(fi->fse, buffer, frames) == false) {
    switch (FLAC__stream_encoder_get_state (fi->fse)) {
    case FLAC__STREAM_ENCODER_OK:
    case FLAC__STREAM_ENCODER_UNINITIALIZED:
      break;
    case FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR:
      return fs_flac_encode_fatal (fi, FISH_SOUND_ERR_OUT_OF_MEMORY);
    default:
      return fs_flac_encode_fatal (fi, FISH_SOUND_ERR_GENERIC);
    }
  }

  fi->packetno++;

  return frames;
}
#else /* ! FS_ENCODE */

#define fs_flac_encode_f NULL
#define fs_flac_encode_f_ilv NULL

#endif /* ! FS_ENCODE */


static FishSound *
fs_flac_delete (FishSound * fsound)
{
  FishSoundFlacInfo * fi = (FishSoundFlacInfo *)fsound->codec_data;
  int i;

  debug_printf("IN");

  if (fsound->mode == FISH_SOUND_DECODE) {
    if (fi->fsd) {
      FLAC__stream_decoder_finish(fi->fsd);
      FLAC__stream_decoder_delete(fi->fsd);
    }
  } else if (fsound->mode == FISH_SOUND_ENCODE) {
    if (fi->fse) {
      FLAC__stream_encoder_finish(fi->fse);
      FLAC__stream_encoder_delete(fi->fse);
    }
    if (fi->buffer) {
      fs_free(fi->buffer);
      fi->buffer = NULL;
    }
  }

  if (fi->ipcm) fs_free(fi->ipcm);
  for (i = 0; i < 8; i++) {
    if (fi->pcm_out[i]) fs_free (fi->pcm_out[i]);
  }
  
#if FS_ENCODE
  if (fi->enc_vc_metadata) {
    fs_flac_metadata_free (fi->enc_vc_metadata);
  }
#endif

  fs_free (fi);
  fsound->codec_data = NULL;

  return fsound;
}

static int
fs_flac_update (FishSound * fsound, int interleave)
{
  return 0;
}

static int
fs_flac_reset (FishSound * fsound)
{
  /*FishSoundFlacInfo * fi = (FishSoundFlacInfo *)fsound->codec_data;*/
#if 0
  if (fsound->mode == FISH_SOUND_DECODE) {
    FLAC__stream_decoder_reset(fi->fsd);
  } else if (fsound->mode == FISH_SOUND_ENCODE) {
  }
#endif
  return 0;
}

static long
fs_flac_flush (FishSound * fsound)
{
  FishSoundFlacInfo * fi = (FishSoundFlacInfo *)fsound->codec_data;

  debug_printf("IN (%s)", fsound->mode == FISH_SOUND_DECODE ? "decode" : "encode");

  if (fsound->mode == FISH_SOUND_DECODE) {
    FLAC__stream_decoder_finish(fi->fsd);
  } else if (fsound->mode == FISH_SOUND_ENCODE) {
    FLAC__stream_encoder_finish(fi->fse);
  }

  return 0;
}

static FishSound *
fs_flac_init (FishSound * fsound)
{
  FishSoundFlacInfo *fi;
  int i;

  fi = fs_malloc (sizeof (FishSoundFlacInfo));
  if (fi == NULL) return NULL;
  fi->fsd = NULL;
  fi->fse = NULL;
  fi->buffer = NULL;
  fi->packetno = 0;
  fi->header = 0;
  fi->header_packets = 0;

  fi->ipcm = NULL;
  for (i = 0; i < 8; i++) {
    fi->pcm_out[i] = NULL;
  }

#if FS_ENCODE
  fi->enc_vc_metadata = NULL;
#endif

  fsound->codec_data = fi;

  return fsound;
}

FishSoundCodec *
fish_sound_flac_codec (void)
{
  FishSoundCodec * codec;

  codec = (FishSoundCodec *) fs_malloc (sizeof (FishSoundCodec));
  if (codec == NULL) return NULL;

  codec->format.format = FISH_SOUND_FLAC;
  codec->format.name = "Flac (Xiph.Org)";
  codec->format.extension = "ogg";

  codec->init = fs_flac_init;
  codec->del = fs_flac_delete;
  codec->reset = fs_flac_reset;
  codec->update = fs_flac_update;
  codec->command = fs_flac_command;
  codec->decode = fs_flac_decode;
  codec->encode_f = fs_flac_encode_f;
  codec->encode_f_ilv = fs_flac_encode_f_ilv;
  codec->flush = fs_flac_flush;

  return codec;
}

#else /* !HAVE_FLAC */

int
fish_sound_flac_identify (unsigned char * buf, long bytes)
{
  return FISH_SOUND_UNKNOWN;
}

FishSoundCodec *
fish_sound_flac_codec (void)
{
  return NULL;
}

#endif
