/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2003                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: mmxfrag.c 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

/*MMX acceleration of fragment reconstruction for motion compensation.
  Originally written by Rudolf Marek.
  Additional optimization by Nils Pipenbrinck.
  Note: Loops are unrolled for best performance.
  The iteration each instruction belongs to is marked in the comments as #i.*/
#include "x86int.h"
#include <stddef.h>

#if defined(USE_ASM)

void oc_frag_recon_intra_mmx(unsigned char *_dst,int _dst_ystride,
 const ogg_int16_t *_residue){
  __asm__ __volatile__(
    /*Set mm0 to 0xFFFFFFFFFFFFFFFF.*/
    "pcmpeqw %%mm0,%%mm0\n\t"
    /*#0 Load low residue.*/
    "movq 0*8(%[residue]),%%mm1\n\t"
    /*#0 Load high residue.*/
    "movq 1*8(%[residue]),%%mm2\n\t"
    /*Set mm0 to 0x8000800080008000.*/
    "psllw $15,%%mm0\n\t"
    /*#1 Load low residue.*/
    "movq 2*8(%[residue]),%%mm3\n\t"
    /*#1 Load high residue.*/
    "movq 3*8(%[residue]),%%mm4\n\t"
    /*Set mm0 to 0x0080008000800080.*/
    "psrlw $8,%%mm0\n\t"
    /*#2 Load low residue.*/
    "movq 4*8(%[residue]),%%mm5\n\t"
    /*#2 Load high residue.*/
    "movq 5*8(%[residue]),%%mm6\n\t"
    /*#0 Bias low  residue.*/
    "paddsw %%mm0,%%mm1\n\t"
    /*#0 Bias high residue.*/
    "paddsw %%mm0,%%mm2\n\t"
    /*#0 Pack to byte.*/
    "packuswb %%mm2,%%mm1\n\t"
    /*#1 Bias low  residue.*/
    "paddsw %%mm0,%%mm3\n\t"
    /*#1 Bias high residue.*/
    "paddsw %%mm0,%%mm4\n\t"
    /*#1 Pack to byte.*/
    "packuswb %%mm4,%%mm3\n\t"
    /*#2 Bias low  residue.*/
    "paddsw %%mm0,%%mm5\n\t"
    /*#2 Bias high residue.*/
    "paddsw %%mm0,%%mm6\n\t"
    /*#2 Pack to byte.*/
    "packuswb %%mm6,%%mm5\n\t"
    /*#0 Write row.*/
    "movq %%mm1,(%[dst])\n\t"
    /*#1 Write row.*/
    "movq %%mm3,(%[dst],%[dst_ystride])\n\t"
    /*#2 Write row.*/
    "movq %%mm5,(%[dst],%[dst_ystride],2)\n\t"
    /*#3 Load low residue.*/
    "movq 6*8(%[residue]),%%mm1\n\t"
    /*#3 Load high residue.*/
    "movq 7*8(%[residue]),%%mm2\n\t"
    /*#4 Load high residue.*/
    "movq 8*8(%[residue]),%%mm3\n\t"
    /*#4 Load high residue.*/
    "movq 9*8(%[residue]),%%mm4\n\t"
    /*#5 Load high residue.*/
    "movq 10*8(%[residue]),%%mm5\n\t"
    /*#5 Load high residue.*/
    "movq 11*8(%[residue]),%%mm6\n\t"
    /*#3 Bias low  residue.*/
    "paddsw %%mm0,%%mm1\n\t"
    /*#3 Bias high residue.*/
    "paddsw %%mm0,%%mm2\n\t"
    /*#3 Pack to byte.*/
    "packuswb %%mm2,%%mm1\n\t"
    /*#4 Bias low  residue.*/
    "paddsw %%mm0,%%mm3\n\t"
    /*#4 Bias high residue.*/
    "paddsw %%mm0,%%mm4\n\t"
    /*#4 Pack to byte.*/
    "packuswb %%mm4,%%mm3\n\t"
    /*#5 Bias low  residue.*/
    "paddsw %%mm0,%%mm5\n\t"
    /*#5 Bias high residue.*/
    "paddsw %%mm0,%%mm6\n\t"
    /*#5 Pack to byte.*/
    "packuswb %%mm6,%%mm5\n\t"
    /*#3 Write row.*/
    "movq %%mm1,(%[dst],%[dst_ystride3])\n\t"
    /*#4 Write row.*/
    "movq %%mm3,(%[dst4])\n\t"
    /*#5 Write row.*/
    "movq %%mm5,(%[dst4],%[dst_ystride])\n\t"
    /*#6 Load low residue.*/
    "movq 12*8(%[residue]),%%mm1\n\t"
    /*#6 Load high residue.*/
    "movq 13*8(%[residue]),%%mm2\n\t"
    /*#7 Load low residue.*/
    "movq 14*8(%[residue]),%%mm3\n\t"
    /*#7 Load high residue.*/
    "movq 15*8(%[residue]),%%mm4\n\t"
    /*#6 Bias low  residue.*/
    "paddsw %%mm0,%%mm1\n\t"
    /*#6 Bias high residue.*/
    "paddsw %%mm0,%%mm2\n\t"
    /*#6 Pack to byte.*/
    "packuswb %%mm2,%%mm1\n\t"
    /*#7 Bias low  residue.*/
    "paddsw %%mm0,%%mm3\n\t"
    /*#7 Bias high residue.*/
    "paddsw %%mm0,%%mm4\n\t"
    /*#7 Pack to byte.*/
    "packuswb %%mm4,%%mm3\n\t"
    /*#6 Write row.*/
    "movq %%mm1,(%[dst4],%[dst_ystride],2)\n\t"
    /*#7 Write row.*/
    "movq %%mm3,(%[dst4],%[dst_ystride3])\n\t"
    :
    :[residue]"r"(_residue),
     [dst]"r"(_dst),
     [dst4]"r"(_dst+(_dst_ystride<<2)),
     [dst_ystride]"r"((ptrdiff_t)_dst_ystride),
     [dst_ystride3]"r"((ptrdiff_t)_dst_ystride*3)
    :"memory"
  );
}

void oc_frag_recon_inter_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src,int _src_ystride,const ogg_int16_t *_residue){
  int i;
  /*Zero mm0.*/
  __asm__ __volatile__("pxor %%mm0,%%mm0\n\t"::);
  for(i=4;i-->0;){
    __asm__ __volatile__(
      /*#0 Load source.*/
      "movq (%[src]),%%mm3\n\t"
      /*#1 Load source.*/
      "movq (%[src],%[src_ystride]),%%mm7\n\t"
      /*#0 Get copy of src.*/
      "movq %%mm3,%%mm4\n\t"
      /*#0 Expand high source.*/
      "punpckhbw %%mm0,%%mm4\n\t"
      /*#0 Expand low  source.*/
      "punpcklbw %%mm0,%%mm3\n\t"
      /*#0 Add residue high.*/
      "paddsw 8(%[residue]),%%mm4\n\t"
      /*#1 Get copy of src.*/
      "movq %%mm7,%%mm2\n\t"
      /*#0 Add residue low.*/
      "paddsw (%[residue]), %%mm3\n\t"
      /*#1 Expand high source.*/
      "punpckhbw %%mm0,%%mm2\n\t"
      /*#0 Pack final row pixels.*/
      "packuswb %%mm4,%%mm3\n\t"
      /*#1 Expand low  source.*/
      "punpcklbw %%mm0,%%mm7\n\t"
      /*#1 Add residue low.*/
      "paddsw 16(%[residue]),%%mm7\n\t"
      /*#1 Add residue high.*/
      "paddsw 24(%[residue]),%%mm2\n\t"
      /*Advance residue.*/
      "lea 32(%[residue]),%[residue]\n\t"
      /*#1 Pack final row pixels.*/
      "packuswb %%mm2,%%mm7\n\t"
      /*Advance src.*/
      "lea (%[src],%[src_ystride],2),%[src]\n\t"
      /*#0 Write row.*/
      "movq %%mm3,(%[dst])\n\t"
      /*#1 Write row.*/
      "movq %%mm7,(%[dst],%[dst_ystride])\n\t"
      /*Advance dst.*/
      "lea (%[dst],%[dst_ystride],2),%[dst]\n\t"
      :[residue]"+r"(_residue),[dst]"+r"(_dst),[src]"+r"(_src)
      :[dst_ystride]"r"((ptrdiff_t)_dst_ystride),
       [src_ystride]"r"((ptrdiff_t)_src_ystride)
      :"memory"
    );
  }
}

void oc_frag_recon_inter2_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src1,int _src1_ystride,const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue){
  int i;
  /*NOTE: This assumes that
     _dst_ystride==_src1_ystride&&_dst_ystride==_src2_ystride.
    This is currently always the case, but a slower fallback version will need
     to be written if it ever is not.*/
  /*Zero mm7.*/
  __asm__ __volatile__("pxor %%mm7,%%mm7\n\t"::);
  for(i=4;i-->0;){
    __asm__ __volatile__(
      /*#0 Load src1.*/
      "movq (%[src1]),%%mm0\n\t"
      /*#0 Load src2.*/
      "movq (%[src2]),%%mm2\n\t"
      /*#0 Copy src1.*/
      "movq %%mm0,%%mm1\n\t"
      /*#0 Copy src2.*/
      "movq %%mm2,%%mm3\n\t"
      /*#1 Load src1.*/
      "movq (%[src1],%[ystride]),%%mm4\n\t"
      /*#0 Unpack lower src1.*/
      "punpcklbw %%mm7,%%mm0\n\t"
      /*#1 Load src2.*/
      "movq (%[src2],%[ystride]),%%mm5\n\t"
      /*#0 Unpack higher src1.*/
      "punpckhbw %%mm7,%%mm1\n\t"
      /*#0 Unpack lower src2.*/
      "punpcklbw %%mm7,%%mm2\n\t"
      /*#0 Unpack higher src2.*/
      "punpckhbw %%mm7,%%mm3\n\t"
      /*Advance src1 ptr.*/
      "lea (%[src1],%[ystride],2),%[src1]\n\t"
      /*Advance src2 ptr.*/
      "lea (%[src2],%[ystride],2),%[src2]\n\t"
      /*#0 Lower src1+src2.*/
      "paddsw %%mm2,%%mm0\n\t"
      /*#0 Higher src1+src2.*/
      "paddsw %%mm3,%%mm1\n\t"
      /*#1 Copy src1.*/
      "movq %%mm4,%%mm2\n\t"
      /*#0 Build lo average.*/
      "psraw $1,%%mm0\n\t"
      /*#1 Copy src2.*/
      "movq %%mm5,%%mm3\n\t"
      /*#1 Unpack lower src1.*/
      "punpcklbw %%mm7,%%mm4\n\t"
      /*#0 Build hi average.*/
      "psraw $1,%%mm1\n\t"
      /*#1 Unpack higher src1.*/
      "punpckhbw %%mm7,%%mm2\n\t"
      /*#0 low+=residue.*/
      "paddsw (%[residue]),%%mm0\n\t"
      /*#1 Unpack lower src2.*/
      "punpcklbw %%mm7,%%mm5\n\t"
      /*#0 high+=residue.*/
      "paddsw 8(%[residue]),%%mm1\n\t"
      /*#1 Unpack higher src2.*/
      "punpckhbw %%mm7,%%mm3\n\t"
      /*#1 Lower src1+src2.*/
      "paddsw %%mm4,%%mm5\n\t"
      /*#0 Pack and saturate.*/
      "packuswb %%mm1,%%mm0\n\t"
      /*#1 Higher src1+src2.*/
      "paddsw %%mm2,%%mm3\n\t"
      /*#0 Write row.*/
      "movq %%mm0,(%[dst])\n\t"
      /*#1 Build lo average.*/
      "psraw $1,%%mm5\n\t"
      /*#1 Build hi average.*/
      "psraw $1,%%mm3\n\t"
      /*#1 low+=residue.*/
      "paddsw 16(%[residue]),%%mm5\n\t"
      /*#1 high+=residue.*/
      "paddsw 24(%[residue]),%%mm3\n\t"
      /*#1 Pack and saturate.*/
      "packuswb  %%mm3,%%mm5\n\t"
      /*#1 Write row ptr.*/
      "movq %%mm5,(%[dst],%[ystride])\n\t"
      /*Advance residue ptr.*/
      "add $32,%[residue]\n\t"
      /*Advance dest ptr.*/
      "lea (%[dst],%[ystride],2),%[dst]\n\t"
     :[dst]"+r"(_dst),[residue]"+r"(_residue),
      [src1]"+r"(_src1),[src2]"+r"(_src2)
     :[ystride]"r"((ptrdiff_t)_dst_ystride)
     :"memory"
    );
  }
}

void oc_restore_fpu_mmx(void){
  __asm__ __volatile__("emms\n\t");
}
#endif
