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
#include "../../internal.h"

/* ------------------------------------------------------------------------
  MMX reconstruction fragment routines for Visual Studio.
  Tested with VS2005. Should compile for VS2003 and VC6 as well.

  Initial implementation 2007 by Nils Pipenbrinck.
  ---------------------------------------------------------------------*/

#if defined(USE_ASM)

void oc_frag_recon_intra_mmx(unsigned char *_dst,int _dst_ystride,
 const ogg_int16_t *_residue){
  /* ---------------------------------------------------------------------
  This function does the inter reconstruction step with 8 iterations
  unrolled. The iteration for each instruction is noted by the #id in the
  comments (in case you want to reconstruct it)
  --------------------------------------------------------------------- */
  _asm{
    mov       edi, [_residue]     /* load residue ptr     */
    mov       eax, 0x00800080     /* generate constant    */
    mov       ebx, [_dst_ystride] /* load dst-stride      */
    mov       edx, [_dst]         /* load dest pointer    */

    /* unrolled loop begins here */

    movd      mm0, eax            /* load constant        */
    movq      mm1, [edi+ 8*0]     /* #1 load low residue  */
    movq      mm2, [edi+ 8*1]     /* #1 load high residue */
    punpckldq mm0, mm0            /* build constant       */
    movq      mm3, [edi+ 8*2]     /* #2 load low residue  */
    movq      mm4, [edi+ 8*3]     /* #2 load high residue */
    movq      mm5, [edi+ 8*4]     /* #3 load low residue  */
    movq      mm6, [edi+ 8*5]     /* #3 load high residue */
    paddsw    mm1, mm0            /* #1 bias low  residue */
    paddsw    mm2, mm0            /* #1 bias high residue */
    packuswb  mm1, mm2            /* #1 pack to byte      */
    paddsw    mm3, mm0            /* #2 bias low  residue */
    paddsw    mm4, mm0            /* #2 bias high residue */
    packuswb  mm3, mm4            /* #2 pack to byte      */
    paddsw    mm5, mm0            /* #3 bias low  residue */
    paddsw    mm6, mm0            /* #3 bias high residue */
    packuswb  mm5, mm6            /* #3 pack to byte      */
    movq      [edx], mm1          /* #1 write row         */
    movq      [edx + ebx], mm3    /* #2 write row         */
    movq      [edx + ebx*2], mm5  /* #3 write row         */
    movq      mm1, [edi+ 8*6]     /* #4 load low residue  */
    lea       ecx, [ebx + ebx*2]  /* make dst_ystride * 3 */
    movq      mm2, [edi+ 8*7]     /* #4 load high residue */
    movq      mm3, [edi+ 8*8]     /* #5 load low residue  */
    lea       esi, [ebx*4 + ebx]  /* make dst_ystride * 5 */
    movq      mm4, [edi+ 8*9]     /* #5 load high residue */
    movq      mm5, [edi+ 8*10]    /* #6 load low residue  */
    lea       eax, [ecx*2 + ebx]  /* make dst_ystride * 7 */
    movq      mm6, [edi+ 8*11]    /* #6 load high residue */
    paddsw    mm1, mm0            /* #4 bias low  residue */
    paddsw    mm2, mm0            /* #4 bias high residue */
    packuswb  mm1, mm2            /* #4 pack to byte      */
    paddsw    mm3, mm0            /* #5 bias low  residue */
    paddsw    mm4, mm0            /* #5 bias high residue */
    packuswb  mm3, mm4            /* #5 pack to byte      */
    paddsw    mm5, mm0            /* #6 bias low  residue */
    paddsw    mm6, mm0            /* #6 bias high residue */
    packuswb  mm5, mm6            /* #6 pack to byte      */
    movq      [edx + ecx], mm1    /* #4 write row         */
    movq      [edx + ebx*4], mm3  /* #5 write row         */
    movq      [edx + esi], mm5    /* #6 write row         */
    movq      mm1, [edi+ 8*12]    /* #7 load low residue  */
    movq      mm2, [edi+ 8*13]    /* #7 load high residue */
    movq      mm3, [edi+ 8*14]    /* #8 load low residue  */
    movq      mm4, [edi+ 8*15]    /* #8 load high residue */
    paddsw    mm1, mm0            /* #7 bias low  residue */
    paddsw    mm2, mm0            /* #7 bias high residue */
    packuswb  mm1, mm2            /* #7 pack to byte      */
    paddsw    mm3, mm0            /* #8 bias low  residue */
    paddsw    mm4, mm0            /* #8 bias high residue */
    packuswb  mm3, mm4            /* #8 pack to byte      */
    movq      [edx + ecx*2], mm1  /* #7 write row         */
    movq      [edx + eax], mm3    /* #8 write row         */
  }
}



void oc_frag_recon_inter_mmx (unsigned char *_dst, int _dst_ystride,
 const unsigned char *_src, int _src_ystride, const ogg_int16_t *_residue){
  /* ---------------------------------------------------------------------
  This function does the inter reconstruction step with two iterations
  running in parallel to hide some load-latencies and break the dependency
  chains. The iteration for each instruction is noted by the #id in the
  comments (in case you want to reconstruct it)
  --------------------------------------------------------------------- */
  _asm{
    pxor      mm0, mm0          /* generate constant 0 */
    mov       esi, [_src]
    mov       edi, [_residue]
    mov       eax, [_src_ystride]
    mov       edx, [_dst]
    mov       ebx, [_dst_ystride]
    mov       ecx, 4

    align 16

nextchunk:
    movq      mm3, [esi]        /* #1 load source        */
    movq      mm1, [edi+0]      /* #1 load residium low  */
    movq      mm2, [edi+8]      /* #1 load residium high */
    movq      mm7, [esi+eax]    /* #2 load source        */
    movq      mm4, mm3          /* #1 get copy of src    */
    movq      mm5, [edi+16]     /* #2 load residium low  */
    punpckhbw mm4, mm0          /* #1 expand high source */
    movq      mm6, [edi+24]     /* #2 load residium high */
    punpcklbw mm3, mm0          /* #1 expand low  source */
    paddsw    mm4, mm2          /* #1 add residium high  */
    movq      mm2, mm7          /* #2 get copy of src    */
    paddsw    mm3, mm1          /* #1 add residium low   */
    punpckhbw mm2, mm0          /* #2 expand high source */
    packuswb  mm3, mm4          /* #1 final row pixels   */
    punpcklbw mm7, mm0          /* #2 expand low  source */
    movq      [edx], mm3        /* #1 write row          */
    paddsw    mm2, mm6          /* #2 add residium high  */
    add       edi, 32           /* residue += 4          */
    paddsw    mm7, mm5          /* #2 add residium low   */
    sub       ecx, 1            /* update loop counter   */
    packuswb  mm7, mm2          /* #2 final row          */
    lea       esi, [esi+eax*2]  /* src += stride * 2     */
    movq      [edx + ebx], mm7  /* #2 write row          */
    lea       edx, [edx+ebx*2]  /* dst += stride * 2     */
    jne       nextchunk
  }
}


void oc_frag_recon_inter2_mmx(unsigned char *_dst,  int _dst_ystride,
 const unsigned char *_src1,  int _src1_ystride, const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue){
  /* ---------------------------------------------------------------------
  This function does the inter2 reconstruction step.The building of the
  average is done with a bit-twiddeling trick to avoid excessive register
  copy work during byte to word conversion.

              average = (a & b) + (((a ^ b) & 0xfe) >> 1);

  (shown for a single byte; it's done with 8 of them at a time)

  Slightly faster than the obvious method using add and shift, but not
  earthshaking improvement either.

  If anyone comes up with a way that produces bit-identical outputs
  using the pavgb instruction let me know and I'll do the 3dnow codepath.
  --------------------------------------------------------------------- */
 _asm{
   mov        eax, 0xfefefefe
   mov        esi, [_src1]
   mov        edi, [_src2]
   movd       mm1, eax
   mov        ebx, [_residue]
   mov        edx, [_dst]
   mov        eax, [_dst_ystride]
   punpckldq  mm1, mm1            /* replicate lsb32     */
   mov        ecx, 8              /* init loop counter   */
   pxor       mm0, mm0            /* constant zero       */
   sub        edx, eax            /* dst -= dst_stride   */

   align      16

nextrow:
   movq       mm2,  [esi]         /* load source1        */
   movq       mm3,  [edi]         /* load source2        */
   movq       mm5,  [ebx + 0]     /* load lower residue  */
   movq       mm6,  [ebx + 8]     /* load higer residue  */
   add        esi,  _src1_ystride /* src1 += src1_stride */
   add        edi,  _src2_ystride /* src2 += src1_stride */
   movq       mm4,  mm2           /* get copy of source1 */
   pand       mm2,  mm3           /* s1 & s2 (avg part)  */
   pxor       mm3,  mm4           /* s1 ^ s2 (avg part)  */
   add        ebx,  16            /* residue++           */
   pand       mm3,  mm1           /* mask out low bits   */
   psrlq      mm3,  1             /* shift xor avg-part  */
   paddd      mm3,  mm2           /* build final average */
   add        edx,  eax           /* dst += dst_stride   */
   movq       mm2,  mm3           /* get copy of average */
   punpckhbw  mm3,  mm0           /* average high        */
   punpcklbw  mm2,  mm0           /* average low         */
   paddsw     mm3,  mm6           /* high + residue      */
   paddsw     mm2,  mm5           /* low  + residue      */
   sub        ecx,  1             /* update loop counter */
   packuswb   mm2,  mm3           /* pack and saturate   */
   movq       [edx], mm2          /* write row           */
   jne        nextrow
 }
}

void oc_restore_fpu_mmx(void){
  _asm { emms }
}

#endif
