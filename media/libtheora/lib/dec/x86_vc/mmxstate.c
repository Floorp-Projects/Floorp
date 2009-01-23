/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2008                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: mmxstate.c 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

/* ------------------------------------------------------------------------
  MMX acceleration of complete fragment reconstruction algorithm.
    Originally written by Rudolf Marek.

  Conversion to MSC intrinsics by Nils Pipenbrinck.
  ---------------------------------------------------------------------*/
#if defined(USE_ASM)

#include "../../internal.h"
#include "../idct.h"
#include "x86int.h"
#include <mmintrin.h>

static const unsigned char OC_FZIG_ZAGMMX[64]=
{
   0, 8, 1, 2, 9,16,24,17,
  10, 3,32,11,18,25, 4,12,
   5,26,19,40,33,34,41,48,
  27, 6,13,20,28,21,14, 7,
  56,49,42,35,43,50,57,36,
  15,22,29,30,23,44,37,58,
  51,59,38,45,52,31,60,53,
  46,39,47,54,61,62,55,63
};

/* Fill a block with value */
static __inline void loc_fill_mmx_value (__m64 * _dst, __m64 _value){
  __m64 t   = _value;
  _dst[0]  = t;  _dst[1]  = t;  _dst[2]  = t;  _dst[3]  = t;
  _dst[4]  = t;  _dst[5]  = t;  _dst[6]  = t;  _dst[7]  = t;
  _dst[8]  = t;  _dst[9]  = t;  _dst[10] = t;  _dst[11] = t;
  _dst[12] = t;  _dst[13] = t;  _dst[14] = t;  _dst[15] = t;
}

/* copy a block of 8 byte elements using different strides */
static __inline void loc_blockcopy_mmx (unsigned char * _dst, int _dst_ystride,
                                        unsigned char * _src, int _src_ystride){
  __m64 a,b,c,d,e,f,g,h;
  a = *(__m64*)(_src + 0 * _src_ystride);
  b = *(__m64*)(_src + 1 * _src_ystride);
  c = *(__m64*)(_src + 2 * _src_ystride);
  d = *(__m64*)(_src + 3 * _src_ystride);
  e = *(__m64*)(_src + 4 * _src_ystride);
  f = *(__m64*)(_src + 5 * _src_ystride);
  g = *(__m64*)(_src + 6 * _src_ystride);
  h = *(__m64*)(_src + 7 * _src_ystride);
  *(__m64*)(_dst + 0 * _dst_ystride) = a;
  *(__m64*)(_dst + 1 * _dst_ystride) = b;
  *(__m64*)(_dst + 2 * _dst_ystride) = c;
  *(__m64*)(_dst + 3 * _dst_ystride) = d;
  *(__m64*)(_dst + 4 * _dst_ystride) = e;
  *(__m64*)(_dst + 5 * _dst_ystride) = f;
  *(__m64*)(_dst + 6 * _dst_ystride) = g;
  *(__m64*)(_dst + 7 * _dst_ystride) = h;
}

void oc_state_frag_recon_mmx(oc_theora_state *_state,const oc_fragment *_frag,
 int _pli,ogg_int16_t _dct_coeffs[128],int _last_zzi,int _ncoefs,
 ogg_uint16_t _dc_iquant,const ogg_uint16_t _ac_iquant[64]){
  ogg_int16_t __declspec(align(16)) res_buf[64];
  int dst_framei;
  int dst_ystride;
  int zzi;
  /*_last_zzi is subtly different from an actual count of the number of
     coefficients we decoded for this block.
    It contains the value of zzi BEFORE the final token in the block was
     decoded.
    In most cases this is an EOB token (the continuation of an EOB run from a
     previous block counts), and so this is the same as the coefficient count.
    However, in the case that the last token was NOT an EOB token, but filled
     the block up with exactly 64 coefficients, _last_zzi will be less than 64.
    Provided the last token was not a pure zero run, the minimum value it can
     be is 46, and so that doesn't affect any of the cases in this routine.
    However, if the last token WAS a pure zero run of length 63, then _last_zzi
     will be 1 while the number of coefficients decoded is 64.
    Thus, we will trigger the following special case, where the real
     coefficient count would not.
    Note also that a zero run of length 64 will give _last_zzi a value of 0,
     but we still process the DC coefficient, which might have a non-zero value
     due to DC prediction.
    Although convoluted, this is arguably the correct behavior: it allows us to
     dequantize fewer coefficients and use a smaller transform when the block
     ends with a long zero run instead of a normal EOB token.
    It could be smarter... multiple separate zero runs at the end of a block
     will fool it, but an encoder that generates these really deserves what it
     gets.
    Needless to say we inherited this approach from VP3.*/
  /*Special case only having a DC component.*/
  if(_last_zzi<2){
    __m64 p;
    /*Why is the iquant product rounded in this case and no others? Who knows.*/
    p = _m_from_int((ogg_int32_t)_frag->dc*_dc_iquant+15>>5);
    /* broadcast 16 bits into all 4 mmx subregisters */
    p = _m_punpcklwd (p,p);
    p = _m_punpckldq (p,p);
    loc_fill_mmx_value ((__m64 *)res_buf, p);
  }
  else{
    /*Then, fill in the remainder of the coefficients with 0's, and perform
       the iDCT.*/
    /*First zero the buffer.*/
    /*On K7, etc., this could be replaced with movntq and sfence.*/
    loc_fill_mmx_value ((__m64 *)res_buf, _mm_setzero_si64());

    res_buf[0]=(ogg_int16_t)((ogg_int32_t)_frag->dc*_dc_iquant);
    /*This is planned to be rewritten in MMX.*/
    for(zzi=1;zzi<_ncoefs;zzi++)
    {
      int ci;
      ci=OC_FZIG_ZAG[zzi];
      res_buf[OC_FZIG_ZAGMMX[zzi]]=(ogg_int16_t)((ogg_int32_t)_dct_coeffs[zzi]*
       _ac_iquant[ci]);
    }

    if(_last_zzi<10){
      oc_idct8x8_10_mmx(res_buf);
    }
    else {
      oc_idct8x8_mmx(res_buf);
    }
  }
  /*Fill in the target buffer.*/
  dst_framei=_state->ref_frame_idx[OC_FRAME_SELF];
  dst_ystride=_state->ref_frame_bufs[dst_framei][_pli].stride;
  /*For now ystride values in all ref frames assumed to be equal.*/
  if(_frag->mbmode==OC_MODE_INTRA){
    oc_frag_recon_intra_mmx(_frag->buffer[dst_framei],dst_ystride,res_buf);
  }
  else{
    int ref_framei;
    int ref_ystride;
    int mvoffsets[2];
    ref_framei=_state->ref_frame_idx[OC_FRAME_FOR_MODE[_frag->mbmode]];
    ref_ystride=_state->ref_frame_bufs[ref_framei][_pli].stride;
    if(oc_state_get_mv_offsets(_state,mvoffsets,_frag->mv[0],
     _frag->mv[1],ref_ystride,_pli)>1){
      oc_frag_recon_inter2_mmx(_frag->buffer[dst_framei],dst_ystride,
       _frag->buffer[ref_framei]+mvoffsets[0],ref_ystride,
       _frag->buffer[ref_framei]+mvoffsets[1],ref_ystride,res_buf);
    }
    else{
      oc_frag_recon_inter_mmx(_frag->buffer[dst_framei],dst_ystride,
       _frag->buffer[ref_framei]+mvoffsets[0],ref_ystride,res_buf);
    }
  }

  _mm_empty();
}


void oc_state_frag_copy_mmx(const oc_theora_state *_state,const int *_fragis,
 int _nfragis,int _dst_frame,int _src_frame,int _pli){
  const int *fragi;
  const int *fragi_end;
  int        dst_framei;
  int        dst_ystride;
  int        src_framei;
  int        src_ystride;
  dst_framei=_state->ref_frame_idx[_dst_frame];
  src_framei=_state->ref_frame_idx[_src_frame];
  dst_ystride=_state->ref_frame_bufs[dst_framei][_pli].stride;
  src_ystride=_state->ref_frame_bufs[src_framei][_pli].stride;
  fragi_end=_fragis+_nfragis;
  for(fragi=_fragis;fragi<fragi_end;fragi++){
    oc_fragment *frag = _state->frags+*fragi;
    loc_blockcopy_mmx (frag->buffer[dst_framei], dst_ystride,
                       frag->buffer[src_framei], src_ystride);
  }
  _m_empty();
}

#endif
