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
 * oggz_auto.c
 *
 * Conrad Parker <conrad@annodex.net>
 */

#ifdef WIN32
#include "config_win32.h"
#else
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "oggz_private.h"
#include "oggz_byteorder.h"

#include <oggz/oggz_stream.h>

/*#define DEBUG*/

/* Allow use of internal metrics; ie. the user_data for these gets free'd
 * when the metric is overwritten, or on close */
int oggz_set_metric_internal (OGGZ * oggz, long serialno, OggzMetric metric,
			      void * user_data, int internal);

int oggz_set_metric_linear (OGGZ * oggz, long serialno,
			    ogg_int64_t granule_rate_numerator,
			    ogg_int64_t granule_rate_denominator);

#define INT16_BE_AT(x) _be_16((*(ogg_int32_t *)(x)))
#define INT32_LE_AT(x) _le_32((*(ogg_int32_t *)(x)))
#define INT32_BE_AT(x) _be_32((*(ogg_int32_t *)(x)))
#define INT64_LE_AT(x) _le_64((*(ogg_int64_t *)(x)))

#define OGGZ_AUTO_MULT 1000Ull

static int
oggz_stream_set_numheaders (OGGZ * oggz, long serialno, int numheaders)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  stream->numheaders = numheaders;

  return 0;
}

static int
auto_speex (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate = 0;
  int numheaders;

  if (op->bytes < 68) return 0;

  granule_rate = (ogg_int64_t) INT32_LE_AT(&header[36]);
#ifdef DEBUG
  printf ("Got speex rate %d\n", (int)granule_rate);
#endif

  oggz_set_granulerate (oggz, serialno, granule_rate, OGGZ_AUTO_MULT);

  numheaders = (ogg_int64_t) INT32_LE_AT(&header[68]) + 2;
  oggz_stream_set_numheaders (oggz, serialno, numheaders);

  return 1;
}

static int
auto_vorbis (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate = 0;

  if (op->bytes < 30) return 0;

  granule_rate = (ogg_int64_t) INT32_LE_AT(&header[12]);
#ifdef DEBUG
  printf ("Got vorbis rate %d\n", (int)granule_rate);
#endif

  oggz_set_granulerate (oggz, serialno, granule_rate, OGGZ_AUTO_MULT);

  oggz_stream_set_numheaders (oggz, serialno, 3);

  return 1;
}

#if USE_THEORA_PRE_ALPHA_3_FORMAT
static int intlog(int num) {
  int ret=0;
  while(num>0){
    num=num/2;
    ret=ret+1;
  }
  return(ret);
}
#endif

static int
auto_theora (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int32_t fps_numerator, fps_denominator;
  char keyframe_granule_shift = 0;
  int keyframe_shift;

  /* TODO: this should check against 42 for the relevant version numbers */
  if (op->bytes < 41) return 0;

  fps_numerator = INT32_BE_AT(&header[22]);
  fps_denominator = INT32_BE_AT(&header[26]);

  /* Very old theora versions used a value of 0 to mean 1.
   * Unfortunately theora hasn't incremented its version field,
   * hence we hardcode this workaround for old or broken streams.
   */
  if (fps_numerator == 0) fps_numerator = 1;

#if USE_THEORA_PRE_ALPHA_3_FORMAT
  /* old header format, used by Theora alpha2 and earlier */
  keyframe_granule_shift = (header[36] & 0xf8) >> 3;
  keyframe_shift = intlog (keyframe_granule_shift - 1);
#else
  keyframe_granule_shift = (char) ((header[40] & 0x03) << 3);
  keyframe_granule_shift |= (header[41] & 0xe0) >> 5; /* see TODO above */
  keyframe_shift = keyframe_granule_shift;
#endif

#ifdef DEBUG
  printf ("Got theora fps %d/%d, keyframe_shift %d\n",
	  fps_numerator, fps_denominator, keyframe_shift);
#endif

  oggz_set_granulerate (oggz, serialno, (ogg_int64_t)fps_numerator,
			OGGZ_AUTO_MULT * (ogg_int64_t)fps_denominator);
  oggz_set_granuleshift (oggz, serialno, keyframe_shift);

  oggz_stream_set_numheaders (oggz, serialno, 3);

  return 1;
}

static int
auto_annodex (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  /* Apply a zero metric */
  oggz_set_granulerate (oggz, serialno, 0, 1);

  return 1;
}

static int
auto_anxdata (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate_numerator = 0, granule_rate_denominator = 0;

  if (op->bytes < 28) return 0;

  granule_rate_numerator = INT64_LE_AT(&header[8]);
  granule_rate_denominator = INT64_LE_AT(&header[16]);
#ifdef DEBUG
  printf ("Got AnxData rate %lld/%lld\n", granule_rate_numerator,
	  granule_rate_denominator);
#endif

  oggz_set_granulerate (oggz, serialno,
			granule_rate_numerator,
			OGGZ_AUTO_MULT * granule_rate_denominator);

  return 1;
}

static int
auto_flac0 (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate = 0;

  granule_rate = (ogg_int64_t) (header[14] << 12) | (header[15] << 4) | 
            ((header[16] >> 4)&0xf);
#ifdef DEBUG
    printf ("Got flac rate %d\n", (int)granule_rate);
#endif
    
  oggz_set_granulerate (oggz, serialno, granule_rate, OGGZ_AUTO_MULT);

  oggz_stream_set_numheaders (oggz, serialno, 3);

  return 1;
}

static int
auto_flac (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate = 0;
  int numheaders;

  if (op->bytes < 51) return 0;

  granule_rate = (ogg_int64_t) (header[27] << 12) | (header[28] << 4) | 
            ((header[29] >> 4)&0xf);
#ifdef DEBUG
  printf ("Got flac rate %d\n", (int)granule_rate);
#endif

  oggz_set_granulerate (oggz, serialno, granule_rate, OGGZ_AUTO_MULT);

  numheaders = INT16_BE_AT(&header[7]);
  oggz_stream_set_numheaders (oggz, serialno, numheaders);

  return 1;
}

/**
 * Recognizer for OggPCM2:
 * http://wiki.xiph.org/index.php/OggPCM2
 */
static int
auto_oggpcm2 (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate;

  if (op->bytes < 28) return 0;

  granule_rate = (ogg_int64_t) INT32_BE_AT(&header[16]);
#ifdef DEBUG
  printf ("Got OggPCM2 rate %d\n", (int)granule_rate);
#endif

  oggz_set_granulerate (oggz, serialno, granule_rate, OGGZ_AUTO_MULT);

  oggz_stream_set_numheaders (oggz, serialno, 3);

  return 1;
}

static int
auto_celt (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate = 0;
  int numheaders;

  if (op->bytes < 56) return 0;

  granule_rate = (ogg_int64_t) INT32_LE_AT(&header[40]);
#ifdef DEBUG
  printf ("Got celt sample rate %d\n", (int)granule_rate);
#endif

  oggz_set_granulerate (oggz, serialno, granule_rate, OGGZ_AUTO_MULT);

  numheaders = (ogg_int64_t) INT32_LE_AT(&header[52]) + 2;
  oggz_stream_set_numheaders (oggz, serialno, numheaders);

  return 1;
}

static int
auto_cmml (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int64_t granule_rate_numerator = 0, granule_rate_denominator = 0;
  int granuleshift;

  if (op->bytes < 28) return 0;

  granule_rate_numerator = INT64_LE_AT(&header[12]);
  granule_rate_denominator = INT64_LE_AT(&header[20]);
  if (op->bytes > 28)
    granuleshift = (int)header[28];
  else
    granuleshift = 0;

#ifdef DEBUG
  printf ("Got CMML rate %lld/%lld\n", granule_rate_numerator,
	  granule_rate_denominator);
#endif

  oggz_set_granulerate (oggz, serialno,
			granule_rate_numerator,
			OGGZ_AUTO_MULT * granule_rate_denominator);
  oggz_set_granuleshift (oggz, serialno, granuleshift);

  oggz_stream_set_numheaders (oggz, serialno, 3);

  return 1;
}

static int
auto_kate (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  ogg_int32_t gps_numerator, gps_denominator;
  unsigned char granule_shift = 0;
  int numheaders;

  if (op->bytes < 64) return 0;

  gps_numerator = INT32_LE_AT(&header[24]);
  gps_denominator = INT32_LE_AT(&header[28]);

  granule_shift = (header[15]);
  numheaders = (header[11]);

#ifdef DEBUG
  printf ("Got kate gps %d/%d, granule shift %d\n",
	  gps_numerator, gps_denominator, granule_shift);
#endif

  oggz_set_granulerate (oggz, serialno, gps_numerator,
			OGGZ_AUTO_MULT * gps_denominator);
  oggz_set_granuleshift (oggz, serialno, granule_shift);
 
  oggz_stream_set_numheaders (oggz, serialno, numheaders);

  return 1;
}

static int
auto_fisbone (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  unsigned char * header = op->packet;
  long fisbone_serialno; /* The serialno referred to in this fisbone */
  ogg_int64_t granule_rate_numerator = 0, granule_rate_denominator = 0;
  int granuleshift, numheaders;

  if (op->bytes < 48) return 0;

  fisbone_serialno = (long) INT32_LE_AT(&header[12]);

  /* Don't override an already assigned metric */
  if (oggz_stream_has_metric (oggz, fisbone_serialno)) return 1;

  granule_rate_numerator = INT64_LE_AT(&header[20]);
  granule_rate_denominator = INT64_LE_AT(&header[28]);
  granuleshift = (int)header[48];

#ifdef DEBUG
  printf ("Got fisbone granulerate %lld/%lld, granuleshift %d for serialno %010ld\n",
	  granule_rate_numerator, granule_rate_denominator, granuleshift,
	  fisbone_serialno);
#endif

  oggz_set_granulerate (oggz, fisbone_serialno,
			granule_rate_numerator,
			OGGZ_AUTO_MULT * granule_rate_denominator);
  oggz_set_granuleshift (oggz, fisbone_serialno, granuleshift);

  /* Increment the number of headers for this stream */
  numheaders = oggz_stream_get_numheaders (oggz, serialno);
  oggz_stream_set_numheaders (oggz, serialno, numheaders+1);
				
  return 1;
}

static int
auto_fishead (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  if (!op->b_o_s)
  {
    return auto_fisbone(oggz, op, serialno, user_data);
  }
  
  oggz_set_granulerate (oggz, serialno, 0, 1);

  /* For skeleton, numheaders will get incremented as each header is seen */
  oggz_stream_set_numheaders (oggz, serialno, 1);
  
  return 1;
}

/*
 * The first two speex packets are header and comment packets (granulepos = 0)
 */

typedef struct {
  int headers_encountered;
  int packet_size;
  int encountered_first_data_packet;
} auto_calc_speex_info_t;

static ogg_int64_t 
auto_calc_speex(ogg_int64_t now, oggz_stream_t *stream, ogg_packet *op) {
  
  /*
   * on the first (b_o_s) packet, set calculate_data to be the number
   * of speex frames per packet
   */

  auto_calc_speex_info_t *info 
          = (auto_calc_speex_info_t *)stream->calculate_data;

  if (stream->calculate_data == NULL) {
    stream->calculate_data = malloc(sizeof(auto_calc_speex_info_t));
    info = stream->calculate_data;
    info->encountered_first_data_packet = 0;
    info->packet_size = 
            (*(int *)(op->packet + 64)) * (*(int *)(op->packet + 56));
    info->headers_encountered = 1;
    return 0;
  }
  
  if (info->headers_encountered < 2) {
    info->headers_encountered += 1;
  } else {
    info->encountered_first_data_packet = 1;
  }

  if (now > -1) {
    return now;
  }

  if (info->encountered_first_data_packet) {
    if (stream->last_granulepos > 0) {
      return stream->last_granulepos + info->packet_size;
    }
    
    return -1;
  }

  return 0;

}

/*
 * The first two CELT packets are header and comment packets (granulepos = 0)
 */

typedef struct {
  int headers_encountered;
  int packet_size;
  int encountered_first_data_packet;
} auto_calc_celt_info_t;

static ogg_int64_t 
auto_calc_celt (ogg_int64_t now, oggz_stream_t *stream, ogg_packet *op) {
  
  /*
   * on the first (b_o_s) packet, set calculate_data to be the number
   * of celt frames per packet
   */

  auto_calc_celt_info_t *info 
          = (auto_calc_celt_info_t *)stream->calculate_data;

  if (stream->calculate_data == NULL) {
    stream->calculate_data = malloc(sizeof(auto_calc_celt_info_t));
    info = stream->calculate_data;
    info->encountered_first_data_packet = 0;

    /* In general, the number of frames per packet depends on the mode.
     * Currently (20080213) both available modes, mono and stereo, have 256
     * frames per packet.
     */
    info->packet_size = 256;

    info->headers_encountered = 1;
    return 0;
  }
  
  if (info->headers_encountered < 2) {
    info->headers_encountered += 1;
  } else {
    info->encountered_first_data_packet = 1;
  }

  if (now > -1) {
    return now;
  }

  if (info->encountered_first_data_packet) {
    if (stream->last_granulepos > 0) {
      return stream->last_granulepos + info->packet_size;
    }
    
    return -1;
  }

  return 0;

}
/*
 * Header packets are marked by a set MSB in the first byte.  Inter packets
 * are marked by a set 2MSB in the first byte.  Intra packets (keyframes)
 * are any that are left over ;-)
 * 
 * (see http://www.theora.org/doc/Theora_I_spec.pdf for the theora 
 * specification)
 */

typedef struct {
  int encountered_first_data_packet;
} auto_calc_theora_info_t;


static ogg_int64_t 
auto_calc_theora(ogg_int64_t now, oggz_stream_t *stream, ogg_packet *op) {

  long keyframe_no;
  int keyframe_shift;
  unsigned char first_byte;
  auto_calc_theora_info_t *info;

  first_byte = op->packet[0];

  info = (auto_calc_theora_info_t *)stream->calculate_data;

  /* header packet */
  if (first_byte & 0x80)
  {
    if (info == NULL) {
      stream->calculate_data = malloc(sizeof(auto_calc_theora_info_t));
      info = stream->calculate_data;
    }
    info->encountered_first_data_packet = 0;
    return (ogg_int64_t)0;
  }
  
  /* known granulepos */
  if (now > (ogg_int64_t)(-1)) {
    info->encountered_first_data_packet = 1;
    return now;
  }

  /* last granulepos unknown */
  if (stream->last_granulepos == -1) {
    info->encountered_first_data_packet = 1;
    return (ogg_int64_t)-1;
  }

  /*
   * first data packet is -1 if gp not set
   */
  if (!info->encountered_first_data_packet) {
    info->encountered_first_data_packet = 1;
    return (ogg_int64_t)-1;
  }

  /* inter-coded packet */
  if (first_byte & 0x40)
  {
    return stream->last_granulepos + 1;
  }

  keyframe_shift = stream->granuleshift; 
  /*
   * retrieve last keyframe number
   */
  keyframe_no = (int)(stream->last_granulepos >> keyframe_shift);
  /*
   * add frames since last keyframe number
   */
  keyframe_no += (stream->last_granulepos & ((1 << keyframe_shift) - 1)) + 1;
  return ((ogg_int64_t)keyframe_no) << keyframe_shift;
  

}

static ogg_int64_t
auto_rcalc_theora(ogg_int64_t next_packet_gp, oggz_stream_t *stream, 
                  ogg_packet *this_packet, ogg_packet *next_packet) {

  int keyframe = (int)(next_packet_gp >> stream->granuleshift);
  int offset = (int)(next_packet_gp - (keyframe << stream->granuleshift));

  /* assume kf is 60 frames ago. NOTE: This is going to cause problems,
   * but I can't think of what else to do.  The position of the last kf
   * is fundamentally unknowable.
   */
  if (offset == 0) {
    return ((keyframe - 60L) << stream->granuleshift) + 59;
  }
  else {
    return (((ogg_int64_t)keyframe) << stream->granuleshift) + (offset - 1);
  }

}


/*
 * Vorbis packets can be short or long, and each packet overlaps the previous
 * and next packets.  The granulepos of a packet is always the last sample
 * that is completely decoded at the end of decoding that packet - i.e. the
 * last packet before the first overlapping packet.  If the sizes of packets
 * are 's' and 'l', then the increment will depend on the previous and next
 * packet types:
 *  v                             prev<<1 | next
 * lll:           l/2             3
 * lls:           3l/4 - s/4      2
 * lsl:           s/2
 * lss:           s/2
 * sll:           l/4 + s/4       1
 * sls:           l/2             0
 * ssl:           s/2
 * sss:           s/2
 *
 * The previous and next packet types can be inferred from the current packet
 * (additional information is not required)
 *
 * The two blocksizes can be determined from the first header packet, by reading
 * byte 28.  1 << (packet[28] >> 4) == long_size.  
 * 1 << (packet[28] & 0xF) == short_size.
 * 
 * (see http://xiph.org/vorbis/doc/Vorbis_I_spec.html for specification)
 */

typedef struct {
  int nln_increments[4];
  int nsn_increment;
  int short_size;
  int long_size;
  int encountered_first_data_packet;
  int last_was_long;
  int log2_num_modes;
  int mode_sizes[1];
} auto_calc_vorbis_info_t;
        

static ogg_int64_t 
auto_calc_vorbis(ogg_int64_t now, oggz_stream_t *stream, ogg_packet *op) {

  auto_calc_vorbis_info_t *info;
  
  if (stream->calculate_data == NULL) {
    /*
     * on the first (b_o_s) packet, determine the long and short sizes,
     * and then calculate l/2, l/4 - s/4, 3 * l/4 - s/4, l/2 - s/2 and s/2
     */
    int short_size;
    int long_size;
  
    long_size = 1 << (op->packet[28] >> 4);
    short_size = 1 << (op->packet[28] & 0xF);

    stream->calculate_data = malloc(sizeof(auto_calc_vorbis_info_t));
    info = (auto_calc_vorbis_info_t *)stream->calculate_data;
    info->nln_increments[3] = long_size >> 1;
    info->nln_increments[2] = 3 * (long_size >> 2) - (short_size >> 2);
    info->nln_increments[1] = (long_size >> 2) + (short_size >> 2);
    info->nln_increments[0] = info->nln_increments[3];
    info->short_size = short_size;
    info->long_size = long_size;
    info->nsn_increment = short_size >> 1;
    info->encountered_first_data_packet = 0;

    /* this is a header packet */
    return 0;
  }

  /*
   * marker for header packets
   */
  if (op->packet[0] & 0x1) {
    /*
     * the code pages, a whole bunch of other fairly useless stuff, AND,
     * RIGHT AT THE END (of a bunch of variable-length compressed rubbish that
     * basically has only one actual set of values that everyone uses BUT YOU 
     * CAN'T BE SURE OF THAT, OH NO YOU CAN'T) is the only piece of data that's
     * actually useful to us - the packet modes (because it's inconceivable to
     * think people might want _just that_ and nothing else, you know, for 
     * seeking and stuff).
     *
     * Fortunately, because of the mandate that non-used bits must be zero
     * at the end of the packet, we might be able to sneakily work backwards
     * and find out the information we need (namely a mapping of modes to
     * packet sizes)
     */
    if (op->packet[0] == 5) {
      unsigned char *current_pos = &op->packet[op->bytes - 1];
      int offset;
      int size;
      int size_check;
      int *mode_size_ptr;
      int i;
      
      /* 
       * This is the format of the mode data at the end of the packet for all
       * Vorbis Version 1 :
       * 
       * [ 6:number_of_modes ]
       * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
       * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
       * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
       * [ 1:framing(1) ]
       *
       * e.g.:
       *
       *              <-
       * 0 0 0 0 0 1 0 0
       * 0 0 1 0 0 0 0 0
       * 0 0 1 0 0 0 0 0
       * 0 0 1|0 0 0 0 0 
       * 0 0 0 0|0|0 0 0
       * 0 0 0 0 0 0 0 0
       * 0 0 0 0|0 0 0 0
       * 0 0 0 0 0 0 0 0
       * 0 0 0 0|0 0 0 0
       * 0 0 0|1|0 0 0 0 |
       * 0 0 0 0 0 0 0 0 V
       * 0 0 0|0 0 0 0 0
       * 0 0 0 0 0 0 0 0
       * 0 0 1|0 0 0 0 0 
       * 0 0|1|0 0 0 0 0 
       * 
       *  
       * i.e. each entry is an important bit, 32 bits of 0, 8 bits of blah, a 
       * bit of 1.
       * Let's find our last 1 bit first.
       *
       */

      size = 0;

      offset = 8;
      while (!((1 << --offset) & *current_pos)) {
        if (offset == 0) {
          offset = 8;
          current_pos -= 1;
        }
      }

      while (1)
      {
        
        /*
         * from current_pos-5:(offset+1) to current_pos-1:(offset+1) should
         * be zero
         */
        offset = (offset + 7) % 8;
        if (offset == 7)
          current_pos -= 1;
        
        if 
        (
          ((current_pos[-5] & ~((1 << (offset + 1)) - 1)) != 0)
          ||
          current_pos[-4] != 0 
          || 
          current_pos[-3] != 0 
          || 
          current_pos[-2] != 0
          ||
          ((current_pos[-1] & ((1 << (offset + 1)) - 1)) != 0)
        )
        {
          break;
        }
        
        size += 1;
        
        current_pos -= 5;
        
      } 

      if (offset > 4) {
        size_check = (current_pos[0] >> (offset - 5)) & 0x3F;
      } else {
        /* mask part of byte from current_pos */
        size_check = (current_pos[0] & ((1 << (offset + 1)) - 1));
        /* shift to appropriate position */
        size_check <<= (5 - offset);
        /* or in part of byte from current_pos - 1 */
        size_check |= (current_pos[-1] & ~((1 << (offset + 3)) - 1)) >> 
                (offset + 3);
      }
      
      size_check += 1;
#ifdef DEBUG
      if (size_check != size)
      {
        printf("WARNING: size parsing failed for VORBIS mode packets\n");
      }
#endif

      /*
       * store mode size information in our info struct
       */
      stream->calculate_data = realloc(stream->calculate_data,
              sizeof(auto_calc_vorbis_info_t) + (size - 1) * sizeof(int));
      info = (auto_calc_vorbis_info_t *)(stream->calculate_data);
      
      i = -1;
      while ((1 << (++i)) < size);
      info->log2_num_modes = i;

      mode_size_ptr = info->mode_sizes;

      for(i = 0; i < size; i++)
      {
        offset = (offset + 1) % 8;
        if (offset == 0)
          current_pos += 1;
        *mode_size_ptr++ = (current_pos[0] >> offset) & 0x1;
        current_pos += 5;
      }
      
    }
    
    return 0;
  }

  info = (auto_calc_vorbis_info_t *)stream->calculate_data;

  return -1;

  { 
    /*
     * we're in a data packet!  First we need to get the mode of the packet,
     * and from the mode, the size
     */
    int mode;
    int size;
    ogg_int64_t result;

    mode = (op->packet[0] >> 1) & ((1 << info->log2_num_modes) - 1);
    size = info->mode_sizes[mode];
  
    /*
     * if we have a working granulepos, we use it, but only if we can't
     * calculate a valid gp value.
     */
    if (now > -1 && stream->last_granulepos == -1) {
      info->encountered_first_data_packet = 1;
      info->last_was_long = size;
      return now;
    }

    if (info->encountered_first_data_packet == 0) {
      info->encountered_first_data_packet = 1;
      info->last_was_long = size;
      return -1;
    }

    /*
     * otherwise, if we haven't yet had a working granulepos, we return
     * -1
     */
    if (stream->last_granulepos == -1) {
      info->last_was_long = size;
      return -1;
    }

    result = stream->last_granulepos + 
      (
        (info->last_was_long ? info->long_size  : info->short_size) 
        + 
        (size ? info->long_size : info->short_size)
      ) / 4;
    info->last_was_long = size;

    return result;
    
  }
  
}

ogg_int64_t
auto_rcalc_vorbis(ogg_int64_t next_packet_gp, oggz_stream_t *stream,
                  ogg_packet *this_packet, ogg_packet *next_packet) {

  auto_calc_vorbis_info_t *info = 
                  (auto_calc_vorbis_info_t *)stream->calculate_data;

  int mode = 
      (this_packet->packet[0] >> 1) & ((1 << info->log2_num_modes) - 1);
  int this_size = info->mode_sizes[mode] ? info->long_size : info->short_size;
  int next_size;
  ogg_int64_t r;

  mode = (next_packet->packet[0] >> 1) & ((1 << info->log2_num_modes) - 1);
  next_size = info->mode_sizes[mode] ? info->long_size : info->short_size;

  r = next_packet_gp - ((this_size + next_size) / 4);
  if (r < 0) return 0L;
  return r;

}

/**
 * FLAC
 * Defined at: http://flac.sourceforge.net/ogg_mapping.html
 *   - Native FLAC audio frames appear as subsequent packets in the stream.
 *     Each packet corresponds to one FLAC audio frame.
 *   - FLAC packets may span page boundaries.
 *
 * The frame header defines block size
 * http://flac.sourceforge.net/format.html#frame_header
 *
 * Note that most FLAC packets will have a granulepos, but rare cases exist
 * where they don't. See for example
 * http://rafb.net/paste/results/Pkib5w72.html
 */

typedef struct {
  ogg_int64_t previous_gp;
  int encountered_first_data_packet;
} auto_calc_flac_info_t;

static ogg_int64_t 
auto_calc_flac (ogg_int64_t now, oggz_stream_t *stream, ogg_packet *op)
{
  auto_calc_flac_info_t *info;

  if (stream->calculate_data == NULL) {
    stream->calculate_data = malloc(sizeof(auto_calc_flac_info_t));
    info = (auto_calc_flac_info_t *)stream->calculate_data;
    info->previous_gp = 0;
    info->encountered_first_data_packet = 0;

    /* this is a header packet */
    goto out;
  }

  info = (auto_calc_flac_info_t *)stream->calculate_data;

  /* FLAC audio frames begin with marker 0xFF */
  if (op->packet[0] == 0xff)
      info->encountered_first_data_packet = 1;

  if (now == -1 && op->packet[0] == 0xff && op->bytes > 2) {
    unsigned char bs;
    int block_size;

    bs = (op->packet[2] & 0xf0) >> 4;

    switch (bs) {
      case 0x0: /*   0000 : get from STREAMINFO metadata block */
        block_size = -1;
        break;
      case 0x1: /* 0001 : 192 samples */
        block_size = 192;
        break;
      /* 0010-0101 : 576 * (2^(n-2)) samples, i.e. 576/1152/2304/4608 */
      case 0x2:
        block_size = 576;
        break;
      case 0x3:
        block_size = 1152;
        break;
      case 0x4:
        block_size = 2304;
        break;
      case 0x5:
        block_size = 4608;
        break;
      case 0x6: /* 0110 : get 8 bit (blocksize-1) from end of header */
        block_size = -1;
        break;
      case 0x7: /* 0111 : get 16 bit (blocksize-1) from end of header */
        block_size = -1;
        break;
      /* 1000-1111 : 256 * (2^(n-8)) samples, i.e. 256/512/1024/2048/4096/8192/16384/32768 */
      case 0x8:
        block_size = 256;
        break;
      case 0x9:
        block_size = 512;
        break;
      case 0xa:
        block_size = 1024;
        break;
      case 0xb:
        block_size = 2048;
        break;
      case 0xc:
        block_size = 4096;
        break;
      case 0xd:
        block_size = 8192;
        break;
      case 0xe:
        block_size = 16384;
        break;
      case 0xf:
        block_size = 32768;
        break;
      default:
        block_size = -1;
        break;
    }

    if (block_size != -1) {
      now = info->previous_gp + block_size;
    }
  } else if (now == -1 && info->encountered_first_data_packet == 0) {
    /* this is a header packet */
    now = 0;
  }

out:
  info->previous_gp = now;
  return now;
}

const oggz_auto_contenttype_t oggz_auto_codec_ident[] = {
  {"\200theora", 7, "Theora", auto_theora, auto_calc_theora, auto_rcalc_theora},
  {"\001vorbis", 7, "Vorbis", auto_vorbis, auto_calc_vorbis, auto_rcalc_vorbis},
  {"Speex", 5, "Speex", auto_speex, auto_calc_speex, NULL},
  {"PCM     ", 8, "PCM", auto_oggpcm2, NULL, NULL},
  {"CMML\0\0\0\0", 8, "CMML", auto_cmml, NULL, NULL},
  {"Annodex", 8, "Annodex", auto_annodex, NULL, NULL},
  {"fishead", 7, "Skeleton", auto_fishead, NULL, NULL},
  {"fLaC", 4, "Flac0", auto_flac0, auto_calc_flac, NULL},
  {"\177FLAC", 4, "Flac", auto_flac, auto_calc_flac, NULL},
  {"AnxData", 7, "AnxData", auto_anxdata, NULL, NULL},
  {"CELT    ", 8, "CELT", auto_celt, auto_calc_celt, NULL},
  {"\200kate\0\0\0", 8, "Kate", auto_kate, NULL, NULL},
  {"", 0, "Unknown", NULL, NULL, NULL}
}; 

static int
oggz_auto_identify (OGGZ * oggz, long serialno, unsigned char * data, long len)
{
  int i;
  
  for (i = 0; i < OGGZ_CONTENT_UNKNOWN; i++)
  {
    const oggz_auto_contenttype_t *codec = oggz_auto_codec_ident + i;
    
    if (len >= codec->bos_str_len &&
        memcmp (data, codec->bos_str, codec->bos_str_len) == 0) {
      
      oggz_stream_set_content (oggz, serialno, i);
      
      return 1;
    }
  }
                      
  oggz_stream_set_content (oggz, serialno, OGGZ_CONTENT_UNKNOWN);
  return 0;
}

int
oggz_auto_identify_page (OGGZ * oggz, ogg_page *og, long serialno)
{
  return oggz_auto_identify (oggz, serialno, og->body, og->body_len);
}

int
oggz_auto_identify_packet (OGGZ * oggz, ogg_packet * op, long serialno)
{
  return oggz_auto_identify (oggz, serialno, op->packet, op->bytes);
}

int
oggz_auto_get_granulerate (OGGZ * oggz, ogg_packet * op, long serialno, 
                void * user_data)
{
  int content = 0;

  content = oggz_stream_get_content(oggz, serialno);
  if (content < 0 || content >= OGGZ_CONTENT_UNKNOWN) {
    return 0;
  }

  oggz_auto_codec_ident[content].reader(oggz, op, serialno, user_data);
  return 0;
}

ogg_int64_t 
oggz_auto_calculate_granulepos(int content, ogg_int64_t now, 
                oggz_stream_t *stream, ogg_packet *op) {
  if (oggz_auto_codec_ident[content].calculator != NULL) {
    ogg_int64_t r = oggz_auto_codec_ident[content].calculator(now, stream, op);
    return r;
  } 

  return now;
}

ogg_int64_t
oggz_auto_calculate_gp_backwards(int content, ogg_int64_t next_packet_gp,
      oggz_stream_t *stream, ogg_packet *this_packet, ogg_packet *next_packet) {

  if (oggz_auto_codec_ident[content].r_calculator != NULL) {
    return oggz_auto_codec_ident[content].r_calculator(next_packet_gp,
            stream, this_packet, next_packet);
  }

  return 0;

}

int
oggz_auto_read_comments (OGGZ * oggz, oggz_stream_t * stream, long serialno,
                         ogg_packet * op)
{
  int offset = -1;
  long len = -1;

  switch (stream->content) {
    case OGGZ_CONTENT_VORBIS:
      if (op->bytes > 7 && memcmp (op->packet, "\003vorbis", 7) == 0)
        offset = 7;
      break;
    case OGGZ_CONTENT_SPEEX:
      offset = 0; break;
    case OGGZ_CONTENT_THEORA:
      if (op->bytes > 7 && memcmp (op->packet, "\201theora", 7) == 0)
        offset = 7;
      break;
    case OGGZ_CONTENT_KATE:
      if (op->bytes > 9 && memcmp (op->packet, "\201kate\0\0\0", 8) == 0) {
        /* we skip the reserved 0 byte after the signature */
        offset = 9;
      }
      break;
    case OGGZ_CONTENT_FLAC:
      if (op->bytes > 4 && (op->packet[0] & 0x7) == 4) {
        len = (op->packet[1]<<16) + (op->packet[2]<<8) + op->packet[3];
        offset = 4;
      }
      break;
    default:
      break;
  }

  /* The length of the comments to decode is the rest of the packet,
   * unless otherwise determined (ie. for FLAC) */
  if (len == -1)
    len = op->bytes - offset;

  if (offset >= 0) {
    oggz_comments_decode (oggz, serialno, op->packet+offset, len);
  }

  return 0;
}

