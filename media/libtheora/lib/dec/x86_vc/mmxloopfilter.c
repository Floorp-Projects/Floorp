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
    last mod: $Id:

 ********************************************************************/

/* -------------------------------------------------------------------
  MMX based loop filter for the theora codec.

  Originally written by Rudolf Marek, based on code from On2's VP3.
  Converted to Visual Studio inline assembly by Nils Pipenbrinck.

  Note: I can't test these since my example files never get into the
  loop filters, but the code has been converted semi-automatic from
  the GCC sources, so it ought to work.
  ---------------------------------------------------------------------*/
#include "../../internal.h"
#include "x86int.h"
#include <mmintrin.h>

#if defined(USE_ASM)



static void loop_filter_v(unsigned char *_pix,int _ystride,
                          const ogg_int16_t *_ll){
  _asm {
    mov       eax,  [_pix]
    mov       edx,  [_ystride]
    mov       ebx,  [_ll]

    /* _pix -= ystride */
    sub       eax,   edx
    /*  mm0=0          */
    pxor      mm0,   mm0
    /* _pix -= ystride */
    sub       eax,   edx
    /*  esi=_ystride*3 */
    lea       esi, [edx + edx*2]

    /*  mm7=_pix[0...8]*/
    movq      mm7, [eax]
    /*  mm4=_pix[0...8+_ystride*3]*/
    movq      mm4, [eax + esi]
    /*  mm6=_pix[0...8]*/
    movq      mm6, mm7
    /*  Expand unsigned _pix[0...3] to 16 bits.*/
    punpcklbw mm6, mm0
    movq      mm5, mm4
    /*  Expand unsigned _pix[4...7] to 16 bits.*/
    punpckhbw mm7, mm0
    punpcklbw mm4, mm0
    /*  Expand other arrays too.*/
    punpckhbw mm5, mm0
    /*mm7:mm6=_p[0...7]-_p[0...7+_ystride*3]:*/
    psubw     mm6, mm4
    psubw     mm7, mm5
    /*mm5=mm4=_pix[0...7+_ystride]*/
    movq      mm4, [eax + edx]
    /*mm1=mm3=mm2=_pix[0..7]+_ystride*2]*/
    movq      mm2, [eax + edx*2]
    movq      mm5, mm4
    movq      mm3, mm2
    movq      mm1, mm2
    /*Expand these arrays.*/
    punpckhbw mm5, mm0
    punpcklbw mm4, mm0
    punpckhbw mm3, mm0
    punpcklbw mm2, mm0
    pcmpeqw   mm0, mm0
    /*mm0=3 3 3 3
    mm3:mm2=_pix[0...8+_ystride*2]-_pix[0...8+_ystride]*/
    psubw     mm3, mm5
    psrlw     mm0, 14
    psubw     mm2, mm4
    /*Scale by 3.*/
    pmullw    mm3, mm0
    pmullw    mm2, mm0
    /*mm0=4 4 4 4
    f=mm3:mm2==_pix[0...8]-_pix[0...8+_ystride*3]+
     3*(_pix[0...8+_ystride*2]-_pix[0...8+_ystride])*/
    psrlw     mm0, 1
    paddw     mm3, mm7
    psllw     mm0, 2
    paddw     mm2, mm6
    /*Add 4.*/
    paddw     mm3, mm0
    paddw     mm2, mm0
    /*"Divide" by 8.*/
    psraw     mm3, 3
    psraw     mm2, 3
    /*Now compute lflim of mm3:mm2 cf. Section 7.10 of the sepc.*/
    /*Free up mm5.*/
    packuswb  mm4, mm5
    /*mm0=L L L L*/
    movq      mm0, [ebx]
    /*if(R_i<-2L||R_i>2L)R_i=0:*/
    movq      mm5, mm2
    pxor      mm6, mm6
    movq      mm7, mm0
    psubw     mm6, mm0
    psllw     mm7, 1
    psllw     mm6, 1
    /*mm2==R_3 R_2 R_1 R_0*/
    /*mm5==R_3 R_2 R_1 R_0*/
    /*mm6==-2L -2L -2L -2L*/
    /*mm7==2L 2L 2L 2L*/
    pcmpgtw   mm7, mm2
    pcmpgtw   mm5, mm6
    pand      mm2, mm7
    movq      mm7, mm0
    pand      mm2, mm5
    psllw     mm7, 1
    movq      mm5, mm3
    /*mm3==R_7 R_6 R_5 R_4*/
    /*mm5==R_7 R_6 R_5 R_4*/
    /*mm6==-2L -2L -2L -2L*/
    /*mm7==2L 2L 2L 2L*/
    pcmpgtw   mm7, mm3
    pcmpgtw   mm5, mm6
    pand      mm3, mm7
    movq      mm7, mm0
    pand      mm3, mm5
   /*if(R_i<-L)R_i'=R_i+2L;
     if(R_i>L)R_i'=R_i-2L;
     if(R_i<-L||R_i>L)R_i=-R_i':*/
    psraw     mm6, 1
    movq      mm5, mm2
    psllw     mm7, 1
    /*mm2==R_3 R_2 R_1 R_0*/
    /*mm5==R_3 R_2 R_1 R_0*/
    /*mm6==-L -L -L -L*/
    /*mm0==L L L L*/
    /*mm5=R_i>L?FF:00*/
    pcmpgtw   mm5, mm0
    /*mm6=-L>R_i?FF:00*/
    pcmpgtw   mm6, mm2
    /*mm7=R_i>L?2L:0*/
    pand      mm7, mm5
    /*mm2=R_i>L?R_i-2L:R_i*/
    psubw     mm2, mm7
    movq      mm7, mm0
    /*mm5=-L>R_i||R_i>L*/
    por       mm5, mm6
    psllw     mm7, 1
    /*mm7=-L>R_i?2L:0*/
    pand      mm7, mm6
    pxor      mm6, mm6
    /*mm2=-L>R_i?R_i+2L:R_i*/
    paddw     mm2, mm7
    psubw     mm6, mm0
    /*mm5=-L>R_i||R_i>L?-R_i':0*/
    pand      mm5, mm2
    movq      mm7, mm0
    /*mm2=-L>R_i||R_i>L?0:R_i*/
    psubw     mm2, mm5
    psllw     mm7, 1
    /*mm2=-L>R_i||R_i>L?-R_i':R_i*/
    psubw     mm2, mm5
    movq      mm5, mm3
    /*mm3==R_7 R_6 R_5 R_4*/
    /*mm5==R_7 R_6 R_5 R_4*/
    /*mm6==-L -L -L -L*/
    /*mm0==L L L L*/
    /*mm6=-L>R_i?FF:00*/
    pcmpgtw   mm6, mm3
    /*mm5=R_i>L?FF:00*/
    pcmpgtw   mm5, mm0
    /*mm7=R_i>L?2L:0*/
    pand      mm7, mm5
    /*mm2=R_i>L?R_i-2L:R_i*/
    psubw     mm3, mm7
    psllw     mm0, 1
    /*mm5=-L>R_i||R_i>L*/
    por       mm5, mm6
    /*mm0=-L>R_i?2L:0*/
    pand      mm0, mm6
    /*mm3=-L>R_i?R_i+2L:R_i*/
    paddw     mm3, mm0
    /*mm5=-L>R_i||R_i>L?-R_i':0*/
    pand      mm5, mm3
    /*mm2=-L>R_i||R_i>L?0:R_i*/
    psubw     mm3, mm5
    /*mm3=-L>R_i||R_i>L?-R_i':R_i*/
    psubw     mm3, mm5
    /*Unfortunately, there's no unsigned byte+signed byte with unsigned
       saturation op code, so we have to promote things back 16 bits.*/
    pxor      mm0, mm0
    movq      mm5, mm4
    punpcklbw mm4, mm0
    punpckhbw mm5, mm0
    movq      mm6, mm1
    punpcklbw mm1, mm0
    punpckhbw mm6, mm0
    /*_pix[0...8+_ystride]+=R_i*/
    paddw     mm4, mm2
    paddw     mm5, mm3
    /*_pix[0...8+_ystride*2]-=R_i*/
    psubw     mm1, mm2
    psubw     mm6, mm3
    packuswb  mm4, mm5
    packuswb  mm1, mm6
    /*Write it back out.*/
    movq    [eax + edx], mm4
    movq    [eax + edx*2], mm1
  }
}

/*This code implements the bulk of loop_filter_h().
  Data are striped p0 p1 p2 p3 ... p0 p1 p2 p3 ..., so in order to load all
   four p0's to one register we must transpose the values in four mmx regs.
  When half is done we repeat this for the rest.*/
static void loop_filter_h4(unsigned char *_pix,long _ystride,
                           const ogg_int16_t *_ll){
  /* todo: merge the comments from the GCC sources */
  _asm {
    mov   ecx, [_pix]
    mov   edx, [_ystride]
    mov   eax, [_ll]
    /*esi=_ystride*3*/
    lea     esi, [edx + edx*2]

    movd    mm0, dword ptr [ecx]
    movd    mm1, dword ptr [ecx + edx]
    movd    mm2, dword ptr [ecx + edx*2]
    movd    mm3, dword ptr [ecx + esi]
    punpcklbw mm0, mm1
    punpcklbw mm2, mm3
    movq    mm1, mm0
    punpckhwd mm0, mm2
    punpcklwd mm1, mm2
    pxor    mm7, mm7
    movq    mm5, mm1
    punpcklbw mm1, mm7
    punpckhbw mm5, mm7
    movq    mm3, mm0
    punpcklbw mm0, mm7
    punpckhbw mm3, mm7
    psubw   mm1, mm3
    movq    mm4, mm0
    pcmpeqw mm2, mm2
    psubw   mm0, mm5
    psrlw   mm2, 14
    pmullw  mm0, mm2
    psrlw   mm2, 1
    paddw   mm0, mm1
    psllw   mm2, 2
    paddw   mm0, mm2
    psraw   mm0, 3
    movq    mm6, qword ptr [eax]
    movq    mm1, mm0
    pxor    mm2, mm2
    movq    mm3, mm6
    psubw   mm2, mm6
    psllw   mm3, 1
    psllw   mm2, 1
    pcmpgtw mm3, mm0
    pcmpgtw mm1, mm2
    pand    mm0, mm3
    pand    mm0, mm1
    psraw   mm2, 1
    movq    mm1, mm0
    movq    mm3, mm6
    pcmpgtw mm2, mm0
    pcmpgtw mm1, mm6
    psllw   mm3, 1
    psllw   mm6, 1
    pand    mm3, mm1
    pand    mm6, mm2
    psubw   mm0, mm3
    por     mm1, mm2
    paddw   mm0, mm6
    pand    mm1, mm0
    psubw   mm0, mm1
    psubw   mm0, mm1
    paddw   mm5, mm0
    psubw   mm4, mm0
    packuswb mm5, mm7
    packuswb mm4, mm7
    punpcklbw mm5, mm4
    movd    edi, mm5
    mov     word ptr [ecx + 01H], di
    psrlq   mm5, 32
    shr     edi, 16
    mov     word ptr [ecx + edx + 01H], di
    movd    edi, mm5
    mov     word ptr [ecx + edx*2 + 01H], di
    shr     edi, 16
    mov     word ptr [ecx + esi + 01H], di
  }
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
  ogg_int16_t __declspec(align(8))        ll[4];
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
  _mm_empty();
}

#endif
