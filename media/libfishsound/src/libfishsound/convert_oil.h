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
 * David Schleef, ds@schleef.org
 * January, 2005
 */

#ifndef __FISH_SOUND_CONVERT_OIL_H__
#define __FISH_SOUND_CONVERT_OIL_H__

#include <string.h>
#include <ogg/ogg.h>
#include <liboil/liboil.h>

/* inline functions */

static inline void
_fs_deinterleave_s_s (short ** src, short * dest[],
		      long frames, int channels)
{
  int j;
  short * s = (short *)src;

#define oil_restride_s16(a,b,c,d,e) oil_conv_u16_s16((uint16_t *)a,b,c,d,e)
  for (j = 0; j < channels; j++) {
    oil_restride_s16 (dest[j], sizeof(short), s + j,
        channels * sizeof(short), frames);
  }
}

static inline void
_fs_deinterleave_s_i (short ** src, int * dest[], long frames, int channels)
{
  int j;
  short * s = (short *)src;

  for (j = 0; j < channels; j++) {
    oil_conv_s32_s16 (dest[j], sizeof(int), s + j,
        channels * sizeof(short), frames);
  }
}

static inline void
_fs_deinterleave_s_f (short ** src, float * dest[], long frames, int channels,
		      float mult)
{
  int j;
  short * s = (short *)src;

  for (j = 0; j < channels; j++) {
    oil_conv_f32_s16 (dest[j], sizeof(float), s + j,
        channels * sizeof(short), frames);
    oil_scalarmult_f32 (dest[j], sizeof (float), dest[j], sizeof(float),
        &mult, frames);
  }
}

static inline void
_fs_deinterleave_s_d (short ** src, double * dest[], long frames, int channels,
		      double mult)
{
  int j;
  short * s = (short *)src;

  for (j = 0; j < channels; j++) {
    oil_conv_f64_s16 (dest[j], sizeof(double), s + j,
        channels * sizeof(short), frames);
    oil_scalarmult_f64 (dest[j], sizeof (double), dest[j], sizeof (double),
        &mult, frames);
  }
}

static inline void
_fs_deinterleave_f_s (float ** src, short * dest[],
		      long frames, int channels, float mult)
{
  int i, j;
  float * s = (float *)src;
  short * d;

  /* FIXME: this needs a temporary buffer for liboil */
  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = (short) (s[i*channels + j] * mult);
    }
  }
}

static inline void
_fs_deinterleave_f_i (float ** src, int * dest[],
		      long frames, int channels, float mult)
{
  int i, j;
  float * s = (float *)src;
  int * d;

  /* FIXME: this needs a temporary buffer for liboil */
  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = (int) (s[i*channels + j] * mult);
    }
  }
}

static inline void
_fs_deinterleave_f_f (float ** src, float * dest[],
		      long frames, int channels, float mult)
{
  int j;
  float * s = (float *)src;

  for (j = 0; j < channels; j++) {
    oil_scalarmult_f32 (dest[j], sizeof(float), s + j,
        channels * sizeof(float), &mult, frames);
  }
}

static inline void
_fs_deinterleave_f_d (float ** src, double * dest[],
		      long frames, int channels, double mult)
{
  int j;
  float * s = (float *)src;

  for (j = 0; j < channels; j++) {
    oil_conv_f64_f32 (dest[j], sizeof(double), s + j,
        channels * sizeof(float), frames);
    oil_scalarmult_f64 (dest[j], sizeof(double), dest[j],
        sizeof(double), &mult, frames);
  }
}

static inline void
_fs_interleave_f_s (float * src[], short ** dest,
		    long frames, int channels, float mult)
{
  int i, j;
  float * s;
  short * d = (short *)dest;

  /* FIXME: this needs a temporary buffer for liboil */
  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = (short) (s[i] * mult);
    }
  }
}

static inline void
_fs_interleave_s_s (short * src[], short ** dest,
		    long frames, int channels)
{
  int j;
  short * d = (short *)dest;

  for (j = 0; j < channels; j++) {
    oil_restride_s16 (d + j, sizeof (short) * channels, src[j],
        sizeof (short), frames);
  }
}

static inline void
_fs_interleave_s_f (short * src[], float ** dest,
		    long frames, int channels, float mult)
{
  int j;
  float * d = (float *)dest;

  for (j = 0; j < channels; j++) {
    oil_conv_f32_s16 (d + j, sizeof (float) * channels, src[j],
        sizeof (short), frames);
  }
  oil_scalarmult_f32 (d, sizeof(float), d, sizeof(float), &mult,
      channels * frames);
}

static inline ogg_int32_t CLIP_TO_15(ogg_int32_t x) {
  int ret=x;
  ret-= ((x<=32767)-1)&(x-32767);
  ret-= ((x>=-32768)-1)&(x+32768);
  return(ret);
}

static inline void
_fs_interleave_i_s (ogg_int32_t * src[], short ** dest,
		    long frames, int channels, int shift)
{
  int i, j;
  ogg_int32_t * s;
  short * d = (short *)dest;

  /* FIXME: shouldn't this use shift? */
  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = (short) CLIP_TO_15(s[i]>>9);
    }
  }
}

static inline void
_fs_interleave_i_f (int * src[], float ** dest,
		    long frames, int channels, float mult)
{
  int j;
  float * d = (float *)dest;

  for (j = 0; j < channels; j++) {
    oil_conv_f32_s32 (d + j, sizeof (float) * channels, src[j],
        sizeof (int), frames);
  }
  oil_scalarmult_f32 (d, sizeof(float), d, sizeof(float), &mult,
      channels * frames);
}

static inline void
_fs_interleave_f_f (float * src[], float ** dest,
		    long frames, int channels, float mult)
{
  int j;
  float * d = (float *)dest;

  for (j = 0; j < channels; j++) {
    oil_scalarmult_f32 (d + j, sizeof (float) * channels, src[j],
        sizeof (float), &mult, frames);
  }
}

static inline void
_fs_interleave_d_s (double * src[], short ** dest,
		    long frames, int channels, double mult)
{
  int i, j;
  double * s;
  short * d = (short *)dest;

  /* FIXME: needs temporary buffer */
  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = (short) (s[i] * mult);
    }
  }
}

static inline void
_fs_interleave_d_f (double * src[], float ** dest,
		    long frames, int channels, float mult)
{
  int j;
  float * d = (float *)dest;

  for (j = 0; j < channels; j++) {
    oil_conv_f32_f64 (d + j, sizeof (float) * channels, src[j],
        sizeof (double), frames);
  }
  oil_scalarmult_f32 (d, sizeof(float), d, sizeof(float), &mult,
      channels * frames);
}

static inline void
_fs_convert_s_s (short * src, short * dest, long samples)
{
  memcpy (dest, src, samples * sizeof (short));
}

static inline void
_fs_convert_s_i (short * src, int * dest, long samples)
{
  oil_conv_s32_s16 (dest, sizeof(int), src, sizeof(short), samples);
}

static inline void
_fs_convert_s_f (short * src, float * dest, long samples, float mult)
{
  oil_conv_f32_s16 (dest, sizeof(float), src, sizeof(short), samples);
  oil_scalarmult_f32 (dest, sizeof(float), dest, sizeof(float), &mult, samples);
}

static inline void
_fs_convert_s_d (short * src, double * dest, long samples, double mult)
{
  oil_conv_f64_s16 (dest, sizeof(double), src, sizeof(short), samples);
  oil_scalarmult_f64 (dest, sizeof(double), dest, sizeof(double), &mult,
      samples);
}

static inline void
_fs_convert_i_s (int * src, short * dest, long samples)
{
  /* FIXME: should this clip? */
  oil_conv_s16_s32 (dest, sizeof(dest), src, sizeof(int), samples);
  /* oil_clipconv_s16_s32 (dest, sizeof(dest), src, sizeof(int), samples); */
}

static inline void
_fs_convert_i_f (int * src, float * dest, long samples, float mult)
{
  oil_conv_f32_s32 (dest, sizeof(float), src, sizeof(int), samples);
  oil_scalarmult_f32 (dest, sizeof(float), dest, sizeof(float), &mult, samples);
}

static inline void
_fs_convert_f_s (float * src, short * dest, long samples, float mult)
{
  int i;

  /* FIXME: needs temp buffer */
  for (i = 0; i < samples; i++) {
    dest[i] = (short) (src[i] * mult);
  }
}

static inline void
_fs_convert_f_i (float * src, int * dest, long samples, float mult)
{
  int i;

  /* FIXME: needs temp buffer */
  for (i = 0; i < samples; i++) {
    dest[i] = (int) (src[i] * mult);
  }
}

static inline void
_fs_convert_f_f (float * src, float * dest, long samples, float mult)
{
  oil_scalarmult_f32 (dest, sizeof(float), src, sizeof(float), &mult, samples);
}

static inline void
_fs_convert_f_d (float * src, double * dest, long samples, double mult)
{
  oil_conv_f64_f32 (dest, sizeof(double), src, sizeof(float), samples);
  oil_scalarmult_f64 (dest, sizeof(double), dest, sizeof(double), &mult, samples);
}

static inline void
_fs_convert_d_s (double * src, short * dest, long samples, double mult)
{
  int i;

  /* FIXME: needs temp buffer */
  for (i = 0; i < samples; i++) {
    dest[i] = (short) (src[i] * mult);
  }
}

static inline void
_fs_convert_d_f (double * src, float * dest, long samples, float mult)
{
  int i;

  /* FIXME: needs temp buffer */
  for (i = 0; i < samples; i++) {
    dest[i] = (float)src[i] * mult;
  }
}

#endif /* __FISH_SOUND_CONVERT_OIL_H__ */
