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

#ifndef __OGGZ_STREAM_PRIVATE_H__
#define __OGGZ_STREAM_PRIVATE_H__

typedef struct _oggz_stream_t oggz_stream_t;

typedef int (*OggzReadBOS) (OGGZ * oggz, long serialno,
                            unsigned char * data, long length,
			    void * user_data);

typedef struct {
  const char      *bos_str;
  int             bos_str_len;
  const char      *content_type;
  OggzReadBOS     reader;
  ogg_int64_t     (*calculator)(ogg_int64_t now, oggz_stream_t *stream, 
                  ogg_packet *op);
  ogg_int64_t     (*r_calculator)(ogg_int64_t next_packet_gp, 
                  oggz_stream_t *stream, ogg_packet *this_packet, 
                  ogg_packet *next_packet);
} oggz_auto_contenttype_t;

extern const oggz_auto_contenttype_t oggz_auto_codec_ident[];


oggz_stream_t * oggz_get_stream (OGGZ * oggz, long serialno);
oggz_stream_t * oggz_add_stream (OGGZ * oggz, long serialno);

int oggz_stream_has_metric (OGGZ * oggz, long serialno);
int oggz_stream_set_content (OGGZ * oggz, long serialno, int content);

ogg_int64_t 
oggz_auto_calculate_granulepos(int content, ogg_int64_t now, 
                oggz_stream_t *stream, ogg_packet *op);

ogg_int64_t
oggz_auto_calculate_gp_backwards(int content, ogg_int64_t next_packet_gp,
      oggz_stream_t *stream, ogg_packet *this_packet, ogg_packet *next_packet);

#endif /* __OGGZ_STREAM_PRIVATE_H__ */
