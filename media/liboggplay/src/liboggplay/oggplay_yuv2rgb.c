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
 * yuv2rgb.c
 *
 * YUV->RGB function, using platform-specific optimisations where possible.
 *
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 * Marcin Lubonski
 */

#include "oggplay_private.h"

/*
 * YUV -> RGB conversion
 *  R = Y + 1.140V
 *  G = Y - 0.395U - 0.581V
 *  B = Y + 2.032U
 *
 * RGB -> YUV conversion
 *  Y = 0.299 R + 0.587 G + 0.114 B
 *  U = 0.147 R - 0.289 G + 0.436 B
 *  V = 0.615 R - 0.515 G - 0.100 B
 */

// Optimized YUV to RGB conversion routine disabled due to generating
// incorrect colours. See Annodex trac ticket 421:
// http://trac.annodex.net/ticket/421
#if 0 //defined(__MMX__) || defined(__SSE__) || defined(__SSE2__) || defined(__SSE3__)

#if defined(WIN32)
#define restrict
#include <emmintrin.h>
#else
#include <xmmintrin.h>
#ifndef restrict
#define restrict __restrict__
#endif
#endif

/* YUV -> RGB Intel MMX implementation */
void oggplay_yuv2rgb(OggPlayYUVChannels * yuv, OggPlayRGBChannels * rgb) {

  int               i;
  unsigned char   * restrict ptry;
  unsigned char   * restrict ptru;
  unsigned char   * restrict ptrv;

  register __m64    *y, *o;
  register __m64    zero, ut, vt, imm, imm2;
  register __m64    r, g, b;
  register __m64    tmp, tmp2;

  zero = _mm_setzero_si64();

  ptry = yuv->ptry;
  ptru = yuv->ptru;
  ptrv = yuv->ptrv;

  for (i = 0; i < yuv->y_height; i++) {
    int j;
    o = (__m64*)rgb->ptro;
    rgb->ptro += rgb->rgb_width * 4;
    for (j = 0; j < yuv->y_width; j += 8) {

      y = (__m64*)&ptry[j];

      ut = _m_from_int(*(int *)(ptru + j/2));
      vt = _m_from_int(*(int *)(ptrv + j/2));

      //ut = _m_from_int(0);
      //vt = _m_from_int(0);

      ut = _m_punpcklbw(ut, zero);
      vt = _m_punpcklbw(vt, zero);

      /* subtract 128 from u and v */
      imm = _mm_set1_pi16(128);
      ut = _m_psubw(ut, imm);
      vt = _m_psubw(vt, imm);

      /* transfer and multiply into r, g, b registers */
      imm = _mm_set1_pi16(-51);
      g = _m_pmullw(ut, imm);
      imm = _mm_set1_pi16(130);
      b = _m_pmullw(ut, imm);
      imm = _mm_set1_pi16(146);
      r = _m_pmullw(vt, imm);
      imm = _mm_set1_pi16(-74);
      imm = _m_pmullw(vt, imm);
      g = _m_paddsw(g, imm);

      /* add 64 to r, g and b registers */
      imm = _mm_set1_pi16(64);
      r = _m_paddsw(r, imm);
      g = _m_paddsw(g, imm);
      imm = _mm_set1_pi16(32);
      b = _m_paddsw(b, imm);

      /* shift r, g and b registers to the right */
      r = _m_psrawi(r, 7);
      g = _m_psrawi(g, 7);
      b = _m_psrawi(b, 6);

      /* subtract 16 from r, g and b registers */
      imm = _mm_set1_pi16(16);
      r = _m_psubsw(r, imm);
      g = _m_psubsw(g, imm);
      b = _m_psubsw(b, imm);

      y = (__m64*)&ptry[j];

      /* duplicate u and v channels and add y
       * each of r,g, b in the form [s1(16), s2(16), s3(16), s4(16)]
       * first interleave, so tmp is [s1(16), s1(16), s2(16), s2(16)]
       * then add y, then interleave again
       * then pack with saturation, to get the desired output of
       *   [s1(8), s1(8), s2(8), s2(8), s3(8), s3(8), s4(8), s4(8)]
       */
      tmp = _m_punpckhwd(r, r);
      imm = _m_punpckhbw(*y, zero);
      //printf("tmp: %llx imm: %llx\n", tmp, imm);
      tmp = _m_paddsw(tmp, imm);
      tmp2 = _m_punpcklwd(r, r);
      imm2 = _m_punpcklbw(*y, zero);
      tmp2 = _m_paddsw(tmp2, imm2);
      r = _m_packuswb(tmp2, tmp);

      tmp = _m_punpckhwd(g, g);
      tmp2 = _m_punpcklwd(g, g);
      tmp = _m_paddsw(tmp, imm);
      tmp2 = _m_paddsw(tmp2, imm2);
      g = _m_packuswb(tmp2, tmp);

      tmp = _m_punpckhwd(b, b);
      tmp2 = _m_punpcklwd(b, b);
      tmp = _m_paddsw(tmp, imm);
      tmp2 = _m_paddsw(tmp2, imm2);
      b = _m_packuswb(tmp2, tmp);
      //printf("duplicated r g and b: %llx %llx %llx\n", r, g, b);

      /* now we have 8 8-bit r, g and b samples.  we want these to be packed
       * into 32-bit values.
       */
      //r = _m_from_int(0);
      //b = _m_from_int(0);
      imm = _mm_set1_pi32(0xFFFFFFFF);
      tmp = _m_punpcklbw(r, b);
      tmp2 = _m_punpcklbw(g, imm);
      *o++ = _m_punpcklbw(tmp, tmp2);
      *o++ = _m_punpckhbw(tmp, tmp2);
      //printf("tmp, tmp2, write1, write2: %llx %llx %llx %llx\n", tmp, tmp2,
      //                _m_punpcklbw(tmp, tmp2), _m_punpckhbw(tmp, tmp2));
      tmp = _m_punpckhbw(r, b);
      tmp2 = _m_punpckhbw(g, imm);
      *o++ = _m_punpcklbw(tmp, tmp2);
      *o++ = _m_punpckhbw(tmp, tmp2);

      //exit(1);
    }
    if (i & 0x1) {
      ptru += yuv->uv_width;
      ptrv += yuv->uv_width;
    }
    ptry += yuv->y_width;
  }
  _m_empty();

}

/* YUV -> BGR Intel MMX implementation */
void oggplay_yuv2bgr(OggPlayYUVChannels * yuv, OggPlayRGBChannels * rgb) {

  int               i;
  unsigned char   * restrict ptry;
  unsigned char   * restrict ptru;
  unsigned char   * restrict ptrv;

  register __m64    *y, *o;
  register __m64    zero, ut, vt, imm, imm2;
  register __m64    r, g, b;
  register __m64    tmp, tmp2;

  zero = _mm_setzero_si64();

  ptry = yuv->ptry;
  ptru = yuv->ptru;
  ptrv = yuv->ptrv;

  for (i = 0; i < yuv->y_height; i++) {
    int j;
    o = (__m64*)rgb->ptro;
    rgb->ptro += rgb->rgb_width * 4;
    for (j = 0; j < yuv->y_width; j += 8) {

      y = (__m64*)&ptry[j];

      ut = _m_from_int(*(int *)(ptru + j/2));
      vt = _m_from_int(*(int *)(ptrv + j/2));

      //ut = _m_from_int(0);
      //vt = _m_from_int(0);

      ut = _m_punpcklbw(ut, zero);
      vt = _m_punpcklbw(vt, zero);

      /* subtract 128 from u and v */
      imm = _mm_set1_pi16(128);
      ut = _m_psubw(ut, imm);
      vt = _m_psubw(vt, imm);

      /* transfer and multiply into r, g, b registers */
      imm = _mm_set1_pi16(-51);
      g = _m_pmullw(ut, imm);
      imm = _mm_set1_pi16(130);
      b = _m_pmullw(ut, imm);
      imm = _mm_set1_pi16(146);
      r = _m_pmullw(vt, imm);
      imm = _mm_set1_pi16(-74);
      imm = _m_pmullw(vt, imm);
      g = _m_paddsw(g, imm);

      /* add 64 to r, g and b registers */
      imm = _mm_set1_pi16(64);
      r = _m_paddsw(r, imm);
      g = _m_paddsw(g, imm);
      imm = _mm_set1_pi16(32);
      b = _m_paddsw(b, imm);

      /* shift r, g and b registers to the right */
      r = _m_psrawi(r, 7);
      g = _m_psrawi(g, 7);
      b = _m_psrawi(b, 6);

      /* subtract 16 from r, g and b registers */
      imm = _mm_set1_pi16(16);
      r = _m_psubsw(r, imm);
      g = _m_psubsw(g, imm);
      b = _m_psubsw(b, imm);

      y = (__m64*)&ptry[j];

      /* duplicate u and v channels and add y
       * each of r,g, b in the form [s1(16), s2(16), s3(16), s4(16)]
       * first interleave, so tmp is [s1(16), s1(16), s2(16), s2(16)]
       * then add y, then interleave again
       * then pack with saturation, to get the desired output of
       *   [s1(8), s1(8), s2(8), s2(8), s3(8), s3(8), s4(8), s4(8)]
       */
      tmp = _m_punpckhwd(r, r);
      imm = _m_punpckhbw(*y, zero);
      //printf("tmp: %llx imm: %llx\n", tmp, imm);
      tmp = _m_paddsw(tmp, imm);
      tmp2 = _m_punpcklwd(r, r);
      imm2 = _m_punpcklbw(*y, zero);
      tmp2 = _m_paddsw(tmp2, imm2);
      r = _m_packuswb(tmp2, tmp);

      tmp = _m_punpckhwd(g, g);
      tmp2 = _m_punpcklwd(g, g);
      tmp = _m_paddsw(tmp, imm);
      tmp2 = _m_paddsw(tmp2, imm2);
      g = _m_packuswb(tmp2, tmp);

      tmp = _m_punpckhwd(b, b);
      tmp2 = _m_punpcklwd(b, b);
      tmp = _m_paddsw(tmp, imm);
      tmp2 = _m_paddsw(tmp2, imm2);
      b = _m_packuswb(tmp2, tmp);
      //printf("duplicated r g and b: %llx %llx %llx\n", r, g, b);

      /* now we have 8 8-bit r, g and b samples.  we want these to be packed
       * into 32-bit values.
       */
      //r = _m_from_int(0);
      //b = _m_from_int(0);
      imm = _mm_set1_pi32(0xFFFFFFFF);
      tmp = _m_punpcklbw(b, r);
      tmp2 = _m_punpcklbw(g, imm);
      *o++ = _m_punpcklbw(tmp, tmp2);
      *o++ = _m_punpckhbw(tmp, tmp2);
      //printf("tmp, tmp2, write1, write2: %llx %llx %llx %llx\n", tmp, tmp2,
      //                _m_punpcklbw(tmp, tmp2), _m_punpckhbw(tmp, tmp2));
      tmp = _m_punpckhbw(b, r);
      tmp2 = _m_punpckhbw(g, imm);
      *o++ = _m_punpcklbw(tmp, tmp2);
      *o++ = _m_punpckhbw(tmp, tmp2);

      //exit(1);
    }
    if (i & 0x1) {
      ptru += yuv->uv_width;
      ptrv += yuv->uv_width;
    }
    ptry += yuv->y_width;
  }
  _m_empty();

}

#elif defined(__xxAPPLExx__)
/*
 * TODO: implement the SIMD method above using Apple's AltiVec code;
 * for now, we'll use the vanilla implementation for Macs.
 *
 * Also, there's probably a better preprocessor macro for detecting
 * the presence of AltiVec than __APPLE__.
 */

/* Macintosh AltiVec implementation */
void oggplay_yuv2rgb(OggPlayYUVChannels * yuv, OggPlayRGBChannels * rgb) {
}

#else

#define CLAMP(v)    ((v) > 255 ? 255 : (v) < 0 ? 0 : (v))

/* Vanilla implementation if YUV->RGB conversion */
void oggplay_yuv2rgb(OggPlayYUVChannels * yuv, OggPlayRGBChannels * rgb) {

  unsigned char * ptry = yuv->ptry;
  unsigned char * ptru = yuv->ptru;
  unsigned char * ptrv = yuv->ptrv;
  unsigned char * ptro = rgb->ptro;
  unsigned char * ptro2;
  int i, j;

  for (i = 0; i < yuv->y_height; i++) {
    ptro2 = ptro;
    for (j = 0; j < yuv->y_width; j += 2) {

      short pr, pg, pb, y;
      short r, g, b;

      pr = (-56992 + ptrv[j/2] * 409) >> 8;
      pg = (34784 - ptru[j/2] * 100 - ptrv[j/2] * 208) >> 8;
      pb = (-70688 + ptru[j/2] * 516) >> 8;

      y = 298*ptry[j] >> 8;
      r = y + pr;
      g = y + pg;
      b = y + pb;

      *ptro2++ = CLAMP(r);
      *ptro2++ = CLAMP(g);
      *ptro2++ = CLAMP(b);
      *ptro2++ = 255;

      y = 298*ptry[j + 1] >> 8;
      r = y + pr;
      g = y + pg;
      b = y + pb;

      *ptro2++ = CLAMP(r);
      *ptro2++ = CLAMP(g);
      *ptro2++ = CLAMP(b);
      *ptro2++ = 255;
    }
    ptry += yuv->y_width;
    if (i & 1) {
      ptru += yuv->uv_width;
      ptrv += yuv->uv_width;
    }
    ptro += rgb->rgb_width * 4;
  }
}

/* Vanilla implementation if YUV->ARGB conversion */
void oggplay_yuv2argb(OggPlayYUVChannels * yuv, OggPlayRGBChannels * rgb) {

  unsigned char * ptry = yuv->ptry;
  unsigned char * ptru = yuv->ptru;
  unsigned char * ptrv = yuv->ptrv;
  unsigned char * ptro = rgb->ptro;
  unsigned char * ptro2;
  int i, j;

  for (i = 0; i < yuv->y_height; i++) {
    ptro2 = ptro;
    for (j = 0; j < yuv->y_width; j += 2) {

      short pr, pg, pb, y;
      short r, g, b;

      pr = (-56992 + ptrv[j/2] * 409) >> 8;
      pg = (34784 - ptru[j/2] * 100 - ptrv[j/2] * 208) >> 8;
      pb = (-70688 + ptru[j/2] * 516) >> 8;

      y = 298*ptry[j] >> 8;
      r = y + pr;
      g = y + pg;
      b = y + pb;

      *ptro2++ = 255;
      *ptro2++ = CLAMP(r);
      *ptro2++ = CLAMP(g);
      *ptro2++ = CLAMP(b);

      y = 298*ptry[j + 1] >> 8;
      r = y + pr;
      g = y + pg;
      b = y + pb;

      *ptro2++ = 255;
      *ptro2++ = CLAMP(r);
      *ptro2++ = CLAMP(g);
      *ptro2++ = CLAMP(b);
    }
    ptry += yuv->y_width;
    if (i & 1) {
      ptru += yuv->uv_width;
      ptrv += yuv->uv_width;
    }
    ptro += rgb->rgb_width * 4;
  }
}


/* Vanilla implementation of YUV->BGR conversion*/
void oggplay_yuv2bgr(OggPlayYUVChannels * yuv, OggPlayRGBChannels * rgb) {

  unsigned char * ptry = yuv->ptry;
  unsigned char * ptru = yuv->ptru;
  unsigned char * ptrv = yuv->ptrv;
  unsigned char * ptro = rgb->ptro;
  unsigned char * ptro2;
  int i, j;

  for (i = 0; i < yuv->y_height; i++) {
    ptro2 = ptro;
    for (j = 0; j < yuv->y_width; j += 2) {

      short pr, pg, pb, y;
      short r, g, b;

      pr = (-56992 + ptrv[j/2] * 409) >> 8;
      pg = (34784 - ptru[j/2] * 100 - ptrv[j/2] * 208) >> 8;
      pb = (-70688 + ptru[j/2] * 516) >> 8;

      y = 298*ptry[j] >> 8;
      r = y + pr;
      g = y + pg;
      b = y + pb;

      *ptro2++ = CLAMP(b);
      *ptro2++ = CLAMP(g);
      *ptro2++ = CLAMP(r);
      *ptro2++ = 255;

      y = 298*ptry[j + 1] >> 8;
      r = y + pr;
      g = y + pg;
      b = y + pb;

      *ptro2++ = CLAMP(b);
      *ptro2++ = CLAMP(g);
      *ptro2++ = CLAMP(r);
      *ptro2++ = 255;
    }
    ptry += yuv->y_width;
    if (i & 1) {
      ptru += yuv->uv_width;
      ptrv += yuv->uv_width;
    }
    ptro += rgb->rgb_width * 4;
  }
}

#endif
