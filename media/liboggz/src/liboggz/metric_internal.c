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

#include "config.h"

#include "oggz_private.h"

#include "oggz/oggz_stream.h"

static ogg_int64_t
oggz_metric_dirac (OGGZ * oggz, long serialno,
                   ogg_int64_t granulepos, void * user_data)
{
  oggz_stream_t * stream;
  ogg_int64_t iframe, pframe;
  ogg_uint32_t pt;
  ogg_uint16_t dist;
  ogg_uint16_t delay;
  ogg_int64_t dt;
  ogg_int64_t units;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return -1;

  iframe = granulepos >> stream->granuleshift;
  pframe = granulepos - (iframe << stream->granuleshift);
  pt = (iframe + pframe) >> 9;
  delay = pframe >> 9;
  dt = (ogg_int64_t)pt - delay;

  units = dt * stream->granulerate_d / stream->granulerate_n;

#ifdef DEBUG
  printf ("oggz_..._granuleshift: serialno %010lu Got frame or field %lld (%lld + %lld): %lld units\n",
	  serialno, dt, iframe, pframe, units);
#endif

  return units;
}

static ogg_int64_t
oggz_metric_default_granuleshift (OGGZ * oggz, long serialno,
				  ogg_int64_t granulepos, void * user_data)
{
  oggz_stream_t * stream;
  ogg_int64_t iframe, pframe;
  ogg_int64_t units;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return -1;

  iframe = granulepos >> stream->granuleshift;
  pframe = granulepos - (iframe << stream->granuleshift);
  granulepos = iframe + pframe;
  if (granulepos > 0) granulepos -= stream->first_granule;

  units = granulepos * stream->granulerate_d / stream->granulerate_n;

#ifdef DEBUG
  printf ("oggz_..._granuleshift: serialno %010lu Got frame %lld (%lld + %lld): %lld units\n",
	  serialno, granulepos, iframe, pframe, units);
#endif

  return units;
}

static ogg_int64_t
oggz_metric_default_linear (OGGZ * oggz, long serialno, ogg_int64_t granulepos,
			    void * user_data)
{
  oggz_stream_t * stream;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return -1;

  return (stream->granulerate_d * granulepos / stream->granulerate_n);
}

static int
oggz_metric_update (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  /* we divide by the granulerate, ie. mult by gr_d/gr_n, so ensure
   * numerator is non-zero */
  if (stream->granulerate_n == 0) {
    stream->granulerate_n= 1;
    stream->granulerate_d = 0;
  }

  if (stream->granuleshift == 0) {
    return oggz_set_metric_internal (oggz, serialno,
				     oggz_metric_default_linear,
				     NULL, 1);
  } else if (oggz_stream_get_content (oggz, serialno) == OGGZ_CONTENT_DIRAC) {
    return oggz_set_metric_internal (oggz, serialno,
				     oggz_metric_dirac,
				     NULL, 1);
  } else {
    return oggz_set_metric_internal (oggz, serialno,
				     oggz_metric_default_granuleshift,
				     NULL, 1);
  }
}

int
oggz_set_granuleshift (OGGZ * oggz, long serialno, int granuleshift)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  stream->granuleshift = granuleshift;

  return oggz_metric_update (oggz, serialno);
}

int
oggz_get_granuleshift (OGGZ * oggz, long serialno)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  return stream->granuleshift;
}

int
oggz_set_granulerate (OGGZ * oggz, long serialno,
		      ogg_int64_t granule_rate_numerator,
		      ogg_int64_t granule_rate_denominator)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  stream->granulerate_n = granule_rate_numerator;
  stream->granulerate_d = granule_rate_denominator;

  return oggz_metric_update (oggz, serialno);
}

int
oggz_get_granulerate (OGGZ * oggz, long serialno,
		      ogg_int64_t * granulerate_n,
		      ogg_int64_t * granulerate_d)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  *granulerate_n = stream->granulerate_n;
  *granulerate_d = stream->granulerate_d / OGGZ_AUTO_MULT;

  return 0;
}

int
oggz_set_first_granule (OGGZ * oggz, long serialno,
		        ogg_int64_t first_granule)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  stream->first_granule = first_granule;

  return oggz_metric_update (oggz, serialno);
}

/** DEPRECATED **/
int
oggz_set_metric_linear (OGGZ * oggz, long serialno,
			ogg_int64_t granule_rate_numerator,
			ogg_int64_t granule_rate_denominator)
{
  oggz_stream_t * stream;

  if (oggz == NULL) return OGGZ_ERR_BAD_OGGZ;

  stream = oggz_get_stream (oggz, serialno);
  if (stream == NULL) return OGGZ_ERR_BAD_SERIALNO;

  stream->granulerate_n = granule_rate_numerator;
  stream->granulerate_d = granule_rate_denominator;
  stream->granuleshift = 0;

  return oggz_metric_update (oggz, serialno);
}

