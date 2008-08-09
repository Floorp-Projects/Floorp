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

#ifndef __FISH_SOUND_CONVERT_C_H__
#define __FISH_SOUND_CONVERT_C_H__

#include <string.h>
#include <ogg/ogg.h>

/* inline functions */

static inline void
_fs_deinterleave_s_s (short ** src, short * dest[],
		      long frames, int channels)
{
  int i, j;
  short * d, * s = (short *)src;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = s[i*channels + j];
    }
  }
}

static inline void
_fs_deinterleave_s_i (short ** src, int * dest[], long frames, int channels)
{
  int i, j;
  short * s = (short *)src;
  int * d;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = (int) s[i*channels + j];
    }
  }
}

static inline void
_fs_deinterleave_s_f (short ** src, float * dest[], long frames, int channels,
		      float mult)
{
  int i, j;
  short * s = (short *)src;
  float * d;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = ((float) s[i*channels + j]) * mult;
    }
  }
}

static inline void
_fs_deinterleave_s_d (short ** src, double * dest[], long frames, int channels,
		      double mult)
{
  int i, j;
  short * s = (short *)src;
  double * d;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = ((double) s[i*channels + j]) * mult;
    }
  }
}

static inline void
_fs_deinterleave_f_s (float ** src, short * dest[],
		      long frames, int channels, float mult)
{
  int i, j;
  float * s = (float *)src;
  short * d;

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
  int i, j;
  float * s = (float *)src, * d;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = s[i*channels + j] * mult;
    }
  }
}

static inline void
_fs_deinterleave_f_d (float ** src, double * dest[],
		      long frames, int channels, double mult)
{
  int i, j;
  float * s = (float *)src;
  double * d;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      d = dest[j];
      d[i] = (double) s[i*channels + j] * mult;
    }
  }
}

static inline void
_fs_interleave_f_s (float * src[], short ** dest,
		    long frames, int channels, float mult)
{
  int i, j;
  float * s;
  short * d = (short *)dest;

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
  int i, j;
  short * s, * d = (short *)dest;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = s[i];
    }
  }
}

static inline void
_fs_interleave_s_f (short * src[], float ** dest,
		    long frames, int channels, float mult)
{
  int i, j;
  short * s;
  float * d = (float *)dest;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = (float) (s[i] * mult);
    }
  }
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
  int i, j;
  int * s;
  float * d = (float *)dest;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = (float) (s[i] * mult);
    }
  }
}

static inline void
_fs_interleave_f_f (float * src[], float ** dest,
		    long frames, int channels, float mult)
{
  int i, j;
  float * s, * d = (float *)dest;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = s[i] * mult;
    }
  }
}

static inline void
_fs_interleave_d_s (double * src[], short ** dest,
		    long frames, int channels, double mult)
{
  int i, j;
  double * s;
  short * d = (short *)dest;

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
  int i, j;
  double * s;
  float * d = (float *)dest;

  for (i = 0; i < frames; i++) {
    for (j = 0; j < channels; j++) {
      s = src[j];
      d[i*channels + j] = (float) s[i] * mult;
    }
  }
}

static inline void
_fs_convert_s_s (short * src, short * dest, long samples)
{
  memcpy (dest, src, samples * sizeof (short));
}

static inline void
_fs_convert_s_i (short * src, int * dest, long samples)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (int) src[i];
  }
}

static inline void
_fs_convert_s_f (short * src, float * dest, long samples, float mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (float) src[i] * mult;
  }
}

static inline void
_fs_convert_s_d (short * src, double * dest, long samples, double mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = ((double)src[i]) * mult;
  }
}

static inline void
_fs_convert_i_s (int * src, short * dest, long samples)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (short) src[i];
  }
}

static inline void
_fs_convert_i_f (int * src, float * dest, long samples, float mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (float) src[i] * mult;
  }
}

static inline void
_fs_convert_f_s (float * src, short * dest, long samples, float mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (short) (src[i] * mult);
  }
}

static inline void
_fs_convert_f_i (float * src, int * dest, long samples, float mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (int) (src[i] * mult);
  }
}

static inline void
_fs_convert_f_f (float * src, float * dest, long samples, float mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = src[i] * mult;
  }
}

static inline void
_fs_convert_f_d (float * src, double * dest, long samples, double mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (double)src[i] * mult;
  }
}

static inline void
_fs_convert_d_s (double * src, short * dest, long samples, double mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (short) (src[i] * mult);
  }
}

static inline void
_fs_convert_d_f (double * src, float * dest, long samples, float mult)
{
  int i;

  for (i = 0; i < samples; i++) {
    dest[i] = (float)src[i] * mult;
  }
}

#endif /* __FISH_SOUND_CONVERT_C_H__ */
