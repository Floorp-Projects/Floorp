/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: mmxstate.c 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

/*MMX acceleration of complete fragment reconstruction algorithm.
  Originally written by Rudolf Marek.*/
#include "x86int.h"
#include "../../internal.h"
#include <stddef.h>

#if defined(USE_ASM)

static const __attribute__((aligned(8),used)) int OC_FZIG_ZAGMMX[64]={
   0, 8, 1, 2, 9,16,24,17,
  10, 3,32,11,18,25, 4,12,
   5,26,19,40,33,34,41,48,
  27, 6,13,20,28,21,14, 7,
  56,49,42,35,43,50,57,36,
  15,22,29,30,23,44,37,58,
  51,59,38,45,52,31,60,53,
  46,39,47,54,61,62,55,63
};



void oc_state_frag_recon_mmx(oc_theora_state *_state,oc_fragment *_frag,
 int _pli,ogg_int16_t _dct_coeffs[128],int _last_zzi,int _ncoefs,
 ogg_uint16_t _dc_iquant,const ogg_uint16_t _ac_iquant[64]){
  ogg_int16_t  __attribute__((aligned(8))) res_buf[64];
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
    ogg_uint16_t p;
    /*Why is the iquant product rounded in this case and no others?
      Who knows.*/
    p=(ogg_int16_t)((ogg_int32_t)_frag->dc*_dc_iquant+15>>5);
    /*Fill res_buf with p.*/
    __asm__ __volatile__(
      /*mm0=0000 0000 0000 AAAA*/
      "movd %[p],%%mm0\n\t"
      /*mm1=0000 0000 0000 AAAA*/
      "movd %[p],%%mm1\n\t"
      /*mm0=0000 0000 AAAA 0000*/
      "pslld $16,%%mm0\n\t"
      /*mm0=0000 0000 AAAA AAAA*/
      "por %%mm1,%%mm0\n\t"
      /*mm0=AAAA AAAA AAAA AAAA*/
      "punpcklwd %%mm0,%%mm0\n\t"
      "movq %%mm0,(%[res_buf])\n\t"
      "movq %%mm0,8(%[res_buf])\n\t"
      "movq %%mm0,16(%[res_buf])\n\t"
      "movq %%mm0,24(%[res_buf])\n\t"
      "movq %%mm0,32(%[res_buf])\n\t"
      "movq %%mm0,40(%[res_buf])\n\t"
      "movq %%mm0,48(%[res_buf])\n\t"
      "movq %%mm0,56(%[res_buf])\n\t"
      "movq %%mm0,64(%[res_buf])\n\t"
      "movq %%mm0,72(%[res_buf])\n\t"
      "movq %%mm0,80(%[res_buf])\n\t"
      "movq %%mm0,88(%[res_buf])\n\t"
      "movq %%mm0,96(%[res_buf])\n\t"
      "movq %%mm0,104(%[res_buf])\n\t"
      "movq %%mm0,112(%[res_buf])\n\t"
      "movq %%mm0,120(%[res_buf])\n\t"
      :
      :[res_buf]"r"(res_buf),[p]"r"((unsigned)p)
      :"memory"
    );
  }
  else{
    /*Then, fill in the remainder of the coefficients with 0's, and perform
       the iDCT.*/
    /*First zero the buffer.*/
    /*On K7, etc., this could be replaced with movntq and sfence.*/
    __asm__ __volatile__(
      "pxor %%mm0,%%mm0\n\t"
      "movq %%mm0,(%[res_buf])\n\t"
      "movq %%mm0,8(%[res_buf])\n\t"
      "movq %%mm0,16(%[res_buf])\n\t"
      "movq %%mm0,24(%[res_buf])\n\t"
      "movq %%mm0,32(%[res_buf])\n\t"
      "movq %%mm0,40(%[res_buf])\n\t"
      "movq %%mm0,48(%[res_buf])\n\t"
      "movq %%mm0,56(%[res_buf])\n\t"
      "movq %%mm0,64(%[res_buf])\n\t"
      "movq %%mm0,72(%[res_buf])\n\t"
      "movq %%mm0,80(%[res_buf])\n\t"
      "movq %%mm0,88(%[res_buf])\n\t"
      "movq %%mm0,96(%[res_buf])\n\t"
      "movq %%mm0,104(%[res_buf])\n\t"
      "movq %%mm0,112(%[res_buf])\n\t"
      "movq %%mm0,120(%[res_buf])\n\t"
      :
      :[res_buf]"r"(res_buf)
      :"memory"
    );
    res_buf[0]=(ogg_int16_t)((ogg_int32_t)_frag->dc*_dc_iquant);
    /*This is planned to be rewritten in MMX.*/
    for(zzi=1;zzi<_ncoefs;zzi++){
      int ci;
      ci=OC_FZIG_ZAG[zzi];
      res_buf[OC_FZIG_ZAGMMX[zzi]]=(ogg_int16_t)((ogg_int32_t)_dct_coeffs[zzi]*
       _ac_iquant[ci]);
    }
    if(_last_zzi<10)oc_idct8x8_10_mmx(res_buf);
    else oc_idct8x8_mmx(res_buf);
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
    if(oc_state_get_mv_offsets(_state,mvoffsets,_frag->mv[0],_frag->mv[1],
     ref_ystride,_pli)>1){
      oc_frag_recon_inter2_mmx(_frag->buffer[dst_framei],dst_ystride,
       _frag->buffer[ref_framei]+mvoffsets[0],ref_ystride,
       _frag->buffer[ref_framei]+mvoffsets[1],ref_ystride,res_buf);
    }
    else{
      oc_frag_recon_inter_mmx(_frag->buffer[dst_framei],dst_ystride,
       _frag->buffer[ref_framei]+mvoffsets[0],ref_ystride,res_buf);
    }
  }
  oc_restore_fpu(_state);
}

/*Copies the fragments specified by the lists of fragment indices from one
   frame to another.
  _fragis:    A pointer to a list of fragment indices.
  _nfragis:   The number of fragment indices to copy.
  _dst_frame: The reference frame to copy to.
  _src_frame: The reference frame to copy from.
  _pli:       The color plane the fragments lie in.*/
void oc_state_frag_copy_mmx(const oc_theora_state *_state,const int *_fragis,
 int _nfragis,int _dst_frame,int _src_frame,int _pli){
  const int *fragi;
  const int *fragi_end;
  int        dst_framei;
  ptrdiff_t  dst_ystride;
  int        src_framei;
  ptrdiff_t  src_ystride;
  dst_framei=_state->ref_frame_idx[_dst_frame];
  src_framei=_state->ref_frame_idx[_src_frame];
  dst_ystride=_state->ref_frame_bufs[dst_framei][_pli].stride;
  src_ystride=_state->ref_frame_bufs[src_framei][_pli].stride;
  fragi_end=_fragis+_nfragis;
  for(fragi=_fragis;fragi<fragi_end;fragi++){
    oc_fragment   *frag;
    unsigned char *dst;
    unsigned char *src;
    ptrdiff_t      s;
    frag=_state->frags+*fragi;
    dst=frag->buffer[dst_framei];
    src=frag->buffer[src_framei];
    __asm__ __volatile__(
      /*src+0*src_ystride*/
      "movq (%[src]),%%mm0\n\t"
      /*s=src_ystride*3*/
      "lea (%[src_ystride],%[src_ystride],2),%[s]\n\t"
      /*src+1*src_ystride*/
      "movq (%[src],%[src_ystride]),%%mm1\n\t"
      /*src+2*src_ystride*/
      "movq (%[src],%[src_ystride],2),%%mm2\n\t"
      /*src+3*src_ystride*/
      "movq (%[src],%[s]),%%mm3\n\t"
      /*dst+0*dst_ystride*/
      "movq %%mm0,(%[dst])\n\t"
      /*s=dst_ystride*3*/
      "lea (%[dst_ystride],%[dst_ystride],2),%[s]\n\t"
      /*dst+1*dst_ystride*/
      "movq %%mm1,(%[dst],%[dst_ystride])\n\t"
      /*Pointer to next 4.*/
      "lea (%[src],%[src_ystride],4),%[src]\n\t"
      /*dst+2*dst_ystride*/
      "movq %%mm2,(%[dst],%[dst_ystride],2)\n\t"
      /*dst+3*dst_ystride*/
      "movq %%mm3,(%[dst],%[s])\n\t"
      /*Pointer to next 4.*/
      "lea (%[dst],%[dst_ystride],4),%[dst]\n\t"
      /*src+0*src_ystride*/
      "movq (%[src]),%%mm0\n\t"
      /*s=src_ystride*3*/
      "lea (%[src_ystride],%[src_ystride],2),%[s]\n\t"
      /*src+1*src_ystride*/
      "movq (%[src],%[src_ystride]),%%mm1\n\t"
      /*src+2*src_ystride*/
      "movq (%[src],%[src_ystride],2),%%mm2\n\t"
      /*src+3*src_ystride*/
      "movq (%[src],%[s]),%%mm3\n\t"
      /*dst+0*dst_ystride*/
      "movq %%mm0,(%[dst])\n\t"
      /*s=dst_ystride*3*/
      "lea (%[dst_ystride],%[dst_ystride],2),%[s]\n\t"
      /*dst+1*dst_ystride*/
      "movq %%mm1,(%[dst],%[dst_ystride])\n\t"
      /*dst+2*dst_ystride*/
      "movq %%mm2,(%[dst],%[dst_ystride],2)\n\t"
      /*dst+3*dst_ystride*/
      "movq %%mm3,(%[dst],%[s])\n\t"
      :[s]"=&r"(s)
      :[dst]"r"(dst),[src]"r"(src),[dst_ystride]"r"(dst_ystride),
       [src_ystride]"r"(src_ystride)
      :"memory"
    );
  }
  /*This needs to be removed when decode specific functions are implemented:*/
  __asm__ __volatile__("emms\n\t");
}

static void loop_filter_v(unsigned char *_pix,int _ystride,
 const ogg_int16_t *_ll){
  ptrdiff_t s;
  _pix-=_ystride*2;
  __asm__ __volatile__(
    /*mm0=0*/
    "pxor %%mm0,%%mm0\n\t"
    /*s=_ystride*3*/
    "lea (%[ystride],%[ystride],2),%[s]\n\t"
    /*mm7=_pix[0...8]*/
    "movq (%[pix]),%%mm7\n\t"
    /*mm4=_pix[0...8+_ystride*3]*/
    "movq (%[pix],%[s]),%%mm4\n\t"
    /*mm6=_pix[0...8]*/
    "movq %%mm7,%%mm6\n\t"
    /*Expand unsigned _pix[0...3] to 16 bits.*/
    "punpcklbw %%mm0,%%mm6\n\t"
    "movq %%mm4,%%mm5\n\t"
    /*Expand unsigned _pix[4...8] to 16 bits.*/
    "punpckhbw %%mm0,%%mm7\n\t"
    /*Expand other arrays too.*/
    "punpcklbw %%mm0,%%mm4\n\t"
    "punpckhbw %%mm0,%%mm5\n\t"
    /*mm7:mm6=_p[0...8]-_p[0...8+_ystride*3]:*/
    "psubw %%mm4,%%mm6\n\t"
    "psubw %%mm5,%%mm7\n\t"
    /*mm5=mm4=_pix[0...8+_ystride]*/
    "movq (%[pix],%[ystride]),%%mm4\n\t"
    /*mm1=mm3=mm2=_pix[0..8]+_ystride*2]*/
    "movq (%[pix],%[ystride],2),%%mm2\n\t"
    "movq %%mm4,%%mm5\n\t"
    "movq %%mm2,%%mm3\n\t"
    "movq %%mm2,%%mm1\n\t"
    /*Expand these arrays.*/
    "punpckhbw %%mm0,%%mm5\n\t"
    "punpcklbw %%mm0,%%mm4\n\t"
    "punpckhbw %%mm0,%%mm3\n\t"
    "punpcklbw %%mm0,%%mm2\n\t"
    /*mm0=3 3 3 3
      mm3:mm2=_pix[0...8+_ystride*2]-_pix[0...8+_ystride]*/
    "pcmpeqw %%mm0,%%mm0\n\t"
    "psubw %%mm5,%%mm3\n\t"
    "psrlw $14,%%mm0\n\t"
    "psubw %%mm4,%%mm2\n\t"
    /*Scale by 3.*/
    "pmullw %%mm0,%%mm3\n\t"
    "pmullw %%mm0,%%mm2\n\t"
    /*mm0=4 4 4 4
      f=mm3:mm2==_pix[0...8]-_pix[0...8+_ystride*3]+
       3*(_pix[0...8+_ystride*2]-_pix[0...8+_ystride])*/
    "psrlw $1,%%mm0\n\t"
    "paddw %%mm7,%%mm3\n\t"
    "psllw $2,%%mm0\n\t"
    "paddw %%mm6,%%mm2\n\t"
    /*Add 4.*/
    "paddw %%mm0,%%mm3\n\t"
    "paddw %%mm0,%%mm2\n\t"
    /*"Divide" by 8.*/
    "psraw $3,%%mm3\n\t"
    "psraw $3,%%mm2\n\t"
    /*Now compute lflim of mm3:mm2 cf. Section 7.10 of the sepc.*/
    /*Free up mm5.*/
    "packuswb %%mm5,%%mm4\n\t"
    /*mm0=L L L L*/
    "movq (%[ll]),%%mm0\n\t"
    /*if(R_i<-2L||R_i>2L)R_i=0:*/
    "movq %%mm2,%%mm5\n\t"
    "pxor %%mm6,%%mm6\n\t"
    "movq %%mm0,%%mm7\n\t"
    "psubw %%mm0,%%mm6\n\t"
    "psllw $1,%%mm7\n\t"
    "psllw $1,%%mm6\n\t"
    /*mm2==R_3 R_2 R_1 R_0*/
    /*mm5==R_3 R_2 R_1 R_0*/
    /*mm6==-2L -2L -2L -2L*/
    /*mm7==2L 2L 2L 2L*/
    "pcmpgtw %%mm2,%%mm7\n\t"
    "pcmpgtw %%mm6,%%mm5\n\t"
    "pand %%mm7,%%mm2\n\t"
    "movq %%mm0,%%mm7\n\t"
    "pand %%mm5,%%mm2\n\t"
    "psllw $1,%%mm7\n\t"
    "movq %%mm3,%%mm5\n\t"
    /*mm3==R_7 R_6 R_5 R_4*/
    /*mm5==R_7 R_6 R_5 R_4*/
    /*mm6==-2L -2L -2L -2L*/
    /*mm7==2L 2L 2L 2L*/
    "pcmpgtw %%mm3,%%mm7\n\t"
    "pcmpgtw %%mm6,%%mm5\n\t"
    "pand %%mm7,%%mm3\n\t"
    "movq %%mm0,%%mm7\n\t"
    "pand %%mm5,%%mm3\n\t"
    /*if(R_i<-L)R_i'=R_i+2L;
      if(R_i>L)R_i'=R_i-2L;
      if(R_i<-L||R_i>L)R_i=-R_i':*/
    "psraw $1,%%mm6\n\t"
    "movq %%mm2,%%mm5\n\t"
    "psllw $1,%%mm7\n\t"
    /*mm2==R_3 R_2 R_1 R_0*/
    /*mm5==R_3 R_2 R_1 R_0*/
    /*mm6==-L -L -L -L*/
    /*mm0==L L L L*/
    /*mm5=R_i>L?FF:00*/
    "pcmpgtw %%mm0,%%mm5\n\t"
    /*mm6=-L>R_i?FF:00*/
    "pcmpgtw %%mm2,%%mm6\n\t"
    /*mm7=R_i>L?2L:0*/
    "pand %%mm5,%%mm7\n\t"
    /*mm2=R_i>L?R_i-2L:R_i*/
    "psubw %%mm7,%%mm2\n\t"
    "movq %%mm0,%%mm7\n\t"
    /*mm5=-L>R_i||R_i>L*/
    "por %%mm6,%%mm5\n\t"
    "psllw $1,%%mm7\n\t"
    /*mm7=-L>R_i?2L:0*/
    "pand %%mm6,%%mm7\n\t"
    "pxor %%mm6,%%mm6\n\t"
    /*mm2=-L>R_i?R_i+2L:R_i*/
    "paddw %%mm7,%%mm2\n\t"
    "psubw %%mm0,%%mm6\n\t"
    /*mm5=-L>R_i||R_i>L?-R_i':0*/
    "pand %%mm2,%%mm5\n\t"
    "movq %%mm0,%%mm7\n\t"
    /*mm2=-L>R_i||R_i>L?0:R_i*/
    "psubw %%mm5,%%mm2\n\t"
    "psllw $1,%%mm7\n\t"
    /*mm2=-L>R_i||R_i>L?-R_i':R_i*/
    "psubw %%mm5,%%mm2\n\t"
    "movq %%mm3,%%mm5\n\t"
    /*mm3==R_7 R_6 R_5 R_4*/
    /*mm5==R_7 R_6 R_5 R_4*/
    /*mm6==-L -L -L -L*/
    /*mm0==L L L L*/
    /*mm6=-L>R_i?FF:00*/
    "pcmpgtw %%mm3,%%mm6\n\t"
    /*mm5=R_i>L?FF:00*/
    "pcmpgtw %%mm0,%%mm5\n\t"
    /*mm7=R_i>L?2L:0*/
    "pand %%mm5,%%mm7\n\t"
    /*mm2=R_i>L?R_i-2L:R_i*/
    "psubw %%mm7,%%mm3\n\t"
    "psllw $1,%%mm0\n\t"
    /*mm5=-L>R_i||R_i>L*/
    "por %%mm6,%%mm5\n\t"
    /*mm0=-L>R_i?2L:0*/
    "pand %%mm6,%%mm0\n\t"
    /*mm3=-L>R_i?R_i+2L:R_i*/
    "paddw %%mm0,%%mm3\n\t"
    /*mm5=-L>R_i||R_i>L?-R_i':0*/
    "pand %%mm3,%%mm5\n\t"
    /*mm2=-L>R_i||R_i>L?0:R_i*/
    "psubw %%mm5,%%mm3\n\t"
    /*mm2=-L>R_i||R_i>L?-R_i':R_i*/
    "psubw %%mm5,%%mm3\n\t"
    /*Unfortunately, there's no unsigned byte+signed byte with unsigned
       saturation op code, so we have to promote things back 16 bits.*/
    "pxor %%mm0,%%mm0\n\t"
    "movq %%mm4,%%mm5\n\t"
    "punpcklbw %%mm0,%%mm4\n\t"
    "punpckhbw %%mm0,%%mm5\n\t"
    "movq %%mm1,%%mm6\n\t"
    "punpcklbw %%mm0,%%mm1\n\t"
    "punpckhbw %%mm0,%%mm6\n\t"
    /*_pix[0...8+_ystride]+=R_i*/
    "paddw %%mm2,%%mm4\n\t"
    "paddw %%mm3,%%mm5\n\t"
    /*_pix[0...8+_ystride*2]-=R_i*/
    "psubw %%mm2,%%mm1\n\t"
    "psubw %%mm3,%%mm6\n\t"
    "packuswb %%mm5,%%mm4\n\t"
    "packuswb %%mm6,%%mm1\n\t"
    /*Write it back out.*/
    "movq %%mm4,(%[pix],%[ystride])\n\t"
    "movq %%mm1,(%[pix],%[ystride],2)\n\t"
    :[s]"=&r"(s)
    :[pix]"r"(_pix),[ystride]"r"((ptrdiff_t)_ystride),[ll]"r"(_ll)
    :"memory"
  );
}

/*This code implements the bulk of loop_filter_h().
  Data are striped p0 p1 p2 p3 ... p0 p1 p2 p3 ..., so in order to load all
   four p0's to one register we must transpose the values in four mmx regs.
  When half is done we repeat this for the rest.*/
static void loop_filter_h4(unsigned char *_pix,ptrdiff_t _ystride,
 const ogg_int16_t *_ll){
  ptrdiff_t s;
  /*d doesn't technically need to be 64-bit on x86-64, but making it so will
     help avoid partial register stalls.*/
  ptrdiff_t d;
  __asm__ __volatile__(
    /*x x x x 3 2 1 0*/
    "movd (%[pix]),%%mm0\n\t"
    /*s=_ystride*3*/
    "lea (%[ystride],%[ystride],2),%[s]\n\t"
    /*x x x x 7 6 5 4*/
    "movd (%[pix],%[ystride]),%%mm1\n\t"
    /*x x x x B A 9 8*/
    "movd (%[pix],%[ystride],2),%%mm2\n\t"
    /*x x x x F E D C*/
    "movd (%[pix],%[s]),%%mm3\n\t"
    /*mm0=7 3 6 2 5 1 4 0*/
    "punpcklbw %%mm1,%%mm0\n\t"
    /*mm2=F B E A D 9 C 8*/
    "punpcklbw %%mm3,%%mm2\n\t"
    /*mm1=7 3 6 2 5 1 4 0*/
    "movq %%mm0,%%mm1\n\t"
    /*mm0=F B 7 3 E A 6 2*/
    "punpckhwd %%mm2,%%mm0\n\t"
    /*mm1=D 9 5 1 C 8 4 0*/
    "punpcklwd %%mm2,%%mm1\n\t"
    "pxor %%mm7,%%mm7\n\t"
    /*mm5=D 9 5 1 C 8 4 0*/
    "movq %%mm1,%%mm5\n\t"
    /*mm1=x C x 8 x 4 x 0==pix[0]*/
    "punpcklbw %%mm7,%%mm1\n\t"
    /*mm5=x D x 9 x 5 x 1==pix[1]*/
    "punpckhbw %%mm7,%%mm5\n\t"
    /*mm3=F B 7 3 E A 6 2*/
    "movq %%mm0,%%mm3\n\t"
    /*mm0=x E x A x 6 x 2==pix[2]*/
    "punpcklbw %%mm7,%%mm0\n\t"
    /*mm3=x F x B x 7 x 3==pix[3]*/
    "punpckhbw %%mm7,%%mm3\n\t"
    /*mm1=mm1-mm3==pix[0]-pix[3]*/
    "psubw %%mm3,%%mm1\n\t"
    /*Save a copy of pix[2] for later.*/
    "movq %%mm0,%%mm4\n\t"
    /*mm2=3 3 3 3
      mm0=mm0-mm5==pix[2]-pix[1]*/
    "pcmpeqw %%mm2,%%mm2\n\t"
    "psubw %%mm5,%%mm0\n\t"
    "psrlw $14,%%mm2\n\t"
    /*Scale by 3.*/
    "pmullw %%mm2,%%mm0\n\t"
    /*mm2=4 4 4 4
      f=mm1==_pix[0]-_pix[3]+ 3*(_pix[2]-_pix[1])*/
    "psrlw $1,%%mm2\n\t"
    "paddw %%mm1,%%mm0\n\t"
    "psllw $2,%%mm2\n\t"
    /*Add 4.*/
    "paddw %%mm2,%%mm0\n\t"
    /*"Divide" by 8, producing the residuals R_i.*/
    "psraw $3,%%mm0\n\t"
    /*Now compute lflim of mm0 cf. Section 7.10 of the sepc.*/
    /*mm6=L L L L*/
    "movq (%[ll]),%%mm6\n\t"
    /*if(R_i<-2L||R_i>2L)R_i=0:*/
    "movq %%mm0,%%mm1\n\t"
    "pxor %%mm2,%%mm2\n\t"
    "movq %%mm6,%%mm3\n\t"
    "psubw %%mm6,%%mm2\n\t"
    "psllw $1,%%mm3\n\t"
    "psllw $1,%%mm2\n\t"
    /*mm0==R_3 R_2 R_1 R_0*/
    /*mm1==R_3 R_2 R_1 R_0*/
    /*mm2==-2L -2L -2L -2L*/
    /*mm3==2L 2L 2L 2L*/
    "pcmpgtw %%mm0,%%mm3\n\t"
    "pcmpgtw %%mm2,%%mm1\n\t"
    "pand %%mm3,%%mm0\n\t"
    "pand %%mm1,%%mm0\n\t"
    /*if(R_i<-L)R_i'=R_i+2L;
      if(R_i>L)R_i'=R_i-2L;
      if(R_i<-L||R_i>L)R_i=-R_i':*/
    "psraw $1,%%mm2\n\t"
    "movq %%mm0,%%mm1\n\t"
    "movq %%mm6,%%mm3\n\t"
    /*mm0==R_3 R_2 R_1 R_0*/
    /*mm1==R_3 R_2 R_1 R_0*/
    /*mm2==-L -L -L -L*/
    /*mm6==L L L L*/
    /*mm2=-L>R_i?FF:00*/
    "pcmpgtw %%mm0,%%mm2\n\t"
    /*mm1=R_i>L?FF:00*/
    "pcmpgtw %%mm6,%%mm1\n\t"
    /*mm3=2L 2L 2L 2L*/
    "psllw $1,%%mm3\n\t"
    /*mm6=2L 2L 2L 2L*/
    "psllw $1,%%mm6\n\t"
    /*mm3=R_i>L?2L:0*/
    "pand %%mm1,%%mm3\n\t"
    /*mm6=-L>R_i?2L:0*/
    "pand %%mm2,%%mm6\n\t"
    /*mm0=R_i>L?R_i-2L:R_i*/
    "psubw %%mm3,%%mm0\n\t"
    /*mm1=-L>R_i||R_i>L*/
    "por %%mm2,%%mm1\n\t"
    /*mm0=-L>R_i?R_i+2L:R_i*/
    "paddw %%mm6,%%mm0\n\t"
    /*mm1=-L>R_i||R_i>L?R_i':0*/
    "pand %%mm0,%%mm1\n\t"
    /*mm0=-L>R_i||R_i>L?0:R_i*/
    "psubw %%mm1,%%mm0\n\t"
    /*mm0=-L>R_i||R_i>L?-R_i':R_i*/
    "psubw %%mm1,%%mm0\n\t"
    /*_pix[1]+=R_i;*/
    "paddw %%mm0,%%mm5\n\t"
    /*_pix[2]-=R_i;*/
    "psubw %%mm0,%%mm4\n\t"
    /*mm5=x x x x D 9 5 1*/
    "packuswb %%mm7,%%mm5\n\t"
    /*mm4=x x x x E A 6 2*/
    "packuswb %%mm7,%%mm4\n\t"
    /*mm5=E D A 9 6 5 2 1*/
    "punpcklbw %%mm4,%%mm5\n\t"
    /*d=6 5 2 1*/
    "movd %%mm5,%[d]\n\t"
    "movw %w[d],1(%[pix])\n\t"
    /*Why is there such a big stall here?*/
    "psrlq $32,%%mm5\n\t"
    "shr $16,%[d]\n\t"
    "movw %w[d],1(%[pix],%[ystride])\n\t"
    /*d=E D A 9*/
    "movd %%mm5,%[d]\n\t"
    "movw %w[d],1(%[pix],%[ystride],2)\n\t"
    "shr $16,%[d]\n\t"
    "movw %w[d],1(%[pix],%[s])\n\t"
    :[s]"=&r"(s),[d]"=&r"(d),
     [pix]"+r"(_pix),[ystride]"+r"(_ystride),[ll]"+r"(_ll)
    :
    :"memory"
  );
}

static void loop_filter_h(unsigned char *_pix,int _ystride,
 const ogg_int16_t *_ll){
  _pix-=2;
  loop_filter_h4(_pix,_ystride,_ll);
  loop_filter_h4(_pix+(_ystride<<2),_ystride,_ll);
}

/*We copy the whole function because the MMX routines will be inlined 4 times,
   and we can do just a single emms call at the end this way.
  We also do not use the _bv lookup table, instead computing the values that
   would lie in it on the fly.*/

/*Apply the loop filter to a given set of fragment rows in the given plane.
  The filter may be run on the bottom edge, affecting pixels in the next row of
   fragments, so this row also needs to be available.
  _bv:        The bounding values array.
  _refi:      The index of the frame buffer to filter.
  _pli:       The color plane to filter.
  _fragy0:    The Y coordinate of the first fragment row to filter.
  _fragy_end: The Y coordinate of the fragment row to stop filtering at.*/
void oc_state_loop_filter_frag_rows_mmx(oc_theora_state *_state,int *_bv,
 int _refi,int _pli,int _fragy0,int _fragy_end){
  ogg_int16_t __attribute__((aligned(8)))  ll[4];
  th_img_plane                            *iplane;
  oc_fragment_plane                       *fplane;
  oc_fragment                             *frag_top;
  oc_fragment                             *frag0;
  oc_fragment                             *frag;
  oc_fragment                             *frag_end;
  oc_fragment                             *frag0_end;
  oc_fragment                             *frag_bot;
  ll[0]=ll[1]=ll[2]=ll[3]=
   (ogg_int16_t)_state->loop_filter_limits[_state->qis[0]];
  iplane=_state->ref_frame_bufs[_refi]+_pli;
  fplane=_state->fplanes+_pli;
  /*The following loops are constructed somewhat non-intuitively on purpose.
    The main idea is: if a block boundary has at least one coded fragment on
     it, the filter is applied to it.
    However, the order that the filters are applied in matters, and VP3 chose
     the somewhat strange ordering used below.*/
  frag_top=_state->frags+fplane->froffset;
  frag0=frag_top+_fragy0*fplane->nhfrags;
  frag0_end=frag0+(_fragy_end-_fragy0)*fplane->nhfrags;
  frag_bot=_state->frags+fplane->froffset+fplane->nfrags;
  while(frag0<frag0_end){
    frag=frag0;
    frag_end=frag+fplane->nhfrags;
    while(frag<frag_end){
      if(frag->coded){
        if(frag>frag0){
          loop_filter_h(frag->buffer[_refi],iplane->stride,ll);
        }
        if(frag0>frag_top){
          loop_filter_v(frag->buffer[_refi],iplane->stride,ll);
        }
        if(frag+1<frag_end&&!(frag+1)->coded){
          loop_filter_h(frag->buffer[_refi]+8,iplane->stride,ll);
        }
        if(frag+fplane->nhfrags<frag_bot&&!(frag+fplane->nhfrags)->coded){
          loop_filter_v((frag+fplane->nhfrags)->buffer[_refi],
           iplane->stride,ll);
        }
      }
      frag++;
    }
    frag0+=fplane->nhfrags;
  }
  /*This needs to be removed when decode specific functions are implemented:*/
  __asm__ __volatile__("emms\n\t");
}

#endif
