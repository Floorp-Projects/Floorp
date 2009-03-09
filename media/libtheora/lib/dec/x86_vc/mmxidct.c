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
  MMX based IDCT for the theora codec.

  Originally written by Rudolf Marek, based on code from On2's VP3.
  Converted to Visual Studio inline assembly by Nils Pipenbrinck.

  ---------------------------------------------------------------------*/
#if defined(USE_ASM)

#include <ogg/ogg.h>
#include "../dct.h"
#include "../idct.h"
#include "x86int.h"

/*A table of constants used by the MMX routines.*/
static const __declspec(align(16)) ogg_uint16_t
 OC_IDCT_CONSTS[(7+1)*4]={
  (ogg_uint16_t)OC_C1S7,(ogg_uint16_t)OC_C1S7,
  (ogg_uint16_t)OC_C1S7,(ogg_uint16_t)OC_C1S7,
  (ogg_uint16_t)OC_C2S6,(ogg_uint16_t)OC_C2S6,
  (ogg_uint16_t)OC_C2S6,(ogg_uint16_t)OC_C2S6,
  (ogg_uint16_t)OC_C3S5,(ogg_uint16_t)OC_C3S5,
  (ogg_uint16_t)OC_C3S5,(ogg_uint16_t)OC_C3S5,
  (ogg_uint16_t)OC_C4S4,(ogg_uint16_t)OC_C4S4,
  (ogg_uint16_t)OC_C4S4,(ogg_uint16_t)OC_C4S4,
  (ogg_uint16_t)OC_C5S3,(ogg_uint16_t)OC_C5S3,
  (ogg_uint16_t)OC_C5S3,(ogg_uint16_t)OC_C5S3,
  (ogg_uint16_t)OC_C6S2,(ogg_uint16_t)OC_C6S2,
  (ogg_uint16_t)OC_C6S2,(ogg_uint16_t)OC_C6S2,
  (ogg_uint16_t)OC_C7S1,(ogg_uint16_t)OC_C7S1,
  (ogg_uint16_t)OC_C7S1,(ogg_uint16_t)OC_C7S1,
      8,    8,    8,    8
};


void oc_idct8x8_10_mmx(ogg_int16_t _y[64]){
  _asm {
    mov     edx, [_y]
    mov     eax, offset OC_IDCT_CONSTS
    movq    mm2, [edx + 30H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 18H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 10H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 38H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 20H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 28H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 10H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 20H], mm6
    movq    mm2, mm0
    movq    mm6, [edx]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 08H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 10H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    movq    mm3, [edx + 20H]
    psubw   mm4, mm7
    paddw   mm1, mm1
    paddw   mm7, mm7
    paddw   mm1, mm2
    paddw   mm7, mm4
    psubw   mm4, mm3
    paddw   mm3, mm3
    psubw   mm6, mm5
    paddw   mm5, mm5
    paddw   mm3, mm4
    paddw   mm5, mm6
    psubw   mm7, mm0
    paddw   mm0, mm0
    movq    [edx + 10H], mm1
    paddw   mm0, mm7
    movq    mm1, mm4
    punpcklwd mm4, mm5
    movq    [edx], mm0
    punpckhwd mm1, mm5
    movq    mm0, mm6
    punpcklwd mm6, mm7
    movq    mm5, mm4
    punpckldq mm4, mm6
    punpckhdq mm5, mm6
    movq    mm6, mm1
    movq    [edx + 08H], mm4
    punpckhwd mm0, mm7
    movq    [edx + 18H], mm5
    punpckhdq mm6, mm0
    movq    mm4, [edx]
    punpckldq mm1, mm0
    movq    mm5, [edx + 10H]
    movq    mm0, mm4
    movq    [edx + 38H], mm6
    punpcklwd mm0, mm5
    movq    [edx + 28H], mm1
    punpckhwd mm4, mm5
    movq    mm5, mm2
    punpcklwd mm2, mm3
    movq    mm1, mm0
    punpckldq mm0, mm2
    punpckhdq mm1, mm2
    movq    mm2, mm4
    movq    [edx], mm0
    punpckhwd mm5, mm3
    movq    [edx + 10H], mm1
    punpckhdq mm4, mm5
    punpckldq mm2, mm5
    movq    [edx + 30H], mm4
    movq    [edx + 20H], mm2
    movq    mm2, [edx + 70H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 58H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 50H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 78H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 60H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 68H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 50H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 60H], mm6
    movq    mm2, mm0
    movq    mm6, [edx + 40H]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 48H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 50H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    movq    mm3, [edx + 60H]
    psubw   mm4, mm7
    paddw   mm1, mm1
    paddw   mm7, mm7
    paddw   mm1, mm2
    paddw   mm7, mm4
    psubw   mm4, mm3
    paddw   mm3, mm3
    psubw   mm6, mm5
    paddw   mm5, mm5
    paddw   mm3, mm4
    paddw   mm5, mm6
    psubw   mm7, mm0
    paddw   mm0, mm0
    movq    [edx + 50H], mm1
    paddw   mm0, mm7
    movq    mm1, mm4
    punpcklwd mm4, mm5
    movq    [edx + 40H], mm0
    punpckhwd mm1, mm5
    movq    mm0, mm6
    punpcklwd mm6, mm7
    movq    mm5, mm4
    punpckldq mm4, mm6
    punpckhdq mm5, mm6
    movq    mm6, mm1
    movq    [edx + 48H], mm4
    punpckhwd mm0, mm7
    movq    [edx + 58H], mm5
    punpckhdq mm6, mm0
    movq    mm4, [edx + 40H]
    punpckldq mm1, mm0
    movq    mm5, [edx + 50H]
    movq    mm0, mm4
    movq    [edx + 78H], mm6
    punpcklwd mm0, mm5
    movq    [edx + 68H], mm1
    punpckhwd mm4, mm5
    movq    mm5, mm2
    punpcklwd mm2, mm3
    movq    mm1, mm0
    punpckldq mm0, mm2
    punpckhdq mm1, mm2
    movq    mm2, mm4
    movq    [edx + 40H], mm0
    punpckhwd mm5, mm3
    movq    [edx + 50H], mm1
    punpckhdq mm4, mm5
    punpckldq mm2, mm5
    movq    [edx + 70H], mm4
    movq    [edx + 60H], mm2
    movq    mm2, [edx + 30H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 50H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 10H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 70H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 20H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 60H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 10H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 20H], mm6
    movq    mm2, mm0
    movq    mm6, [edx]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 40H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 10H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    paddw   mm2, [eax + 38H]
    paddw   mm1, mm1
    paddw   mm1, mm2
    psraw   mm2, 4
    psubw   mm4, mm7
    psraw   mm1, 4
    movq    mm3, [edx + 20H]
    paddw   mm7, mm7
    movq    [edx + 20H], mm2
    paddw   mm7, mm4
    movq    [edx + 10H], mm1
    psubw   mm4, mm3
    paddw   mm4, [eax + 38H]
    paddw   mm3, mm3
    paddw   mm3, mm4
    psraw   mm4, 4
    psubw   mm6, mm5
    psraw   mm3, 4
    paddw   mm6, [eax + 38H]
    paddw   mm5, mm5
    paddw   mm5, mm6
    psraw   mm6, 4
    movq    [edx + 40H], mm4
    psraw   mm5, 4
    movq    [edx + 30H], mm3
    psubw   mm7, mm0
    paddw   mm7, [eax + 38H]
    paddw   mm0, mm0
    paddw   mm0, mm7
    psraw   mm7, 4
    movq    [edx + 60H], mm6
    psraw   mm0, 4
    movq    [edx + 50H], mm5
    movq    [edx + 70H], mm7
    movq    [edx], mm0
    movq    mm2, [edx + 38H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 58H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 18H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 78H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 28H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 68H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 18H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 28H], mm6
    movq    mm2, mm0
    movq    mm6, [edx + 08H]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 48H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 18H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    paddw   mm2, [eax + 38H]
    paddw   mm1, mm1
    paddw   mm1, mm2
    psraw   mm2, 4
    psubw   mm4, mm7
    psraw   mm1, 4
    movq    mm3, [edx + 28H]
    paddw   mm7, mm7
    movq    [edx + 28H], mm2
    paddw   mm7, mm4
    movq    [edx + 18H], mm1
    psubw   mm4, mm3
    paddw   mm4, [eax + 38H]
    paddw   mm3, mm3
    paddw   mm3, mm4
    psraw   mm4, 4
    psubw   mm6, mm5
    psraw   mm3, 4
    paddw   mm6, [eax + 38H]
    paddw   mm5, mm5
    paddw   mm5, mm6
    psraw   mm6, 4
    movq    [edx + 48H], mm4
    psraw   mm5, 4
    movq    [edx + 38H], mm3
    psubw   mm7, mm0
    paddw   mm7, [eax + 38H]
    paddw   mm0, mm0
    paddw   mm0, mm7
    psraw   mm7, 4
    movq    [edx + 68H], mm6
    psraw   mm0, 4
    movq    [edx + 58H], mm5
    movq    [edx + 78H], mm7
    movq    [edx + 08H], mm0
    /* emms  */
  }
}


void oc_idct8x8_mmx(ogg_int16_t _y[64]){
  _asm {
    mov     edx, [_y]
    mov     eax, offset OC_IDCT_CONSTS
    movq    mm2, [edx + 30H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 18H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 10H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 38H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 20H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 28H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 10H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 20H], mm6
    movq    mm2, mm0
    movq    mm6, [edx]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 08H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 10H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    movq    mm3, [edx + 20H]
    psubw   mm4, mm7
    paddw   mm1, mm1
    paddw   mm7, mm7
    paddw   mm1, mm2
    paddw   mm7, mm4
    psubw   mm4, mm3
    paddw   mm3, mm3
    psubw   mm6, mm5
    paddw   mm5, mm5
    paddw   mm3, mm4
    paddw   mm5, mm6
    psubw   mm7, mm0
    paddw   mm0, mm0
    movq    [edx + 10H], mm1
    paddw   mm0, mm7
    movq    mm1, mm4
    punpcklwd mm4, mm5
    movq    [edx], mm0
    punpckhwd mm1, mm5
    movq    mm0, mm6
    punpcklwd mm6, mm7
    movq    mm5, mm4
    punpckldq mm4, mm6
    punpckhdq mm5, mm6
    movq    mm6, mm1
    movq    [edx + 08H], mm4
    punpckhwd mm0, mm7
    movq    [edx + 18H], mm5
    punpckhdq mm6, mm0
    movq    mm4, [edx]
    punpckldq mm1, mm0
    movq    mm5, [edx + 10H]
    movq    mm0, mm4
    movq    [edx + 38H], mm6
    punpcklwd mm0, mm5
    movq    [edx + 28H], mm1
    punpckhwd mm4, mm5
    movq    mm5, mm2
    punpcklwd mm2, mm3
    movq    mm1, mm0
    punpckldq mm0, mm2
    punpckhdq mm1, mm2
    movq    mm2, mm4
    movq    [edx], mm0
    punpckhwd mm5, mm3
    movq    [edx + 10H], mm1
    punpckhdq mm4, mm5
    punpckldq mm2, mm5
    movq    [edx + 30H], mm4
    movq    [edx + 20H], mm2
    movq    mm2, [edx + 70H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 58H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 50H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 78H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 60H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 68H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 50H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 60H], mm6
    movq    mm2, mm0
    movq    mm6, [edx + 40H]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 48H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 50H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    movq    mm3, [edx + 60H]
    psubw   mm4, mm7
    paddw   mm1, mm1
    paddw   mm7, mm7
    paddw   mm1, mm2
    paddw   mm7, mm4
    psubw   mm4, mm3
    paddw   mm3, mm3
    psubw   mm6, mm5
    paddw   mm5, mm5
    paddw   mm3, mm4
    paddw   mm5, mm6
    psubw   mm7, mm0
    paddw   mm0, mm0
    movq    [edx + 50H], mm1
    paddw   mm0, mm7
    movq    mm1, mm4
    punpcklwd mm4, mm5
    movq    [edx + 40H], mm0
    punpckhwd mm1, mm5
    movq    mm0, mm6
    punpcklwd mm6, mm7
    movq    mm5, mm4
    punpckldq mm4, mm6
    punpckhdq mm5, mm6
    movq    mm6, mm1
    movq    [edx + 48H], mm4
    punpckhwd mm0, mm7
    movq    [edx + 58H], mm5
    punpckhdq mm6, mm0
    movq    mm4, [edx + 40H]
    punpckldq mm1, mm0
    movq    mm5, [edx + 50H]
    movq    mm0, mm4
    movq    [edx + 78H], mm6
    punpcklwd mm0, mm5
    movq    [edx + 68H], mm1
    punpckhwd mm4, mm5
    movq    mm5, mm2
    punpcklwd mm2, mm3
    movq    mm1, mm0
    punpckldq mm0, mm2
    punpckhdq mm1, mm2
    movq    mm2, mm4
    movq    [edx + 40H], mm0
    punpckhwd mm5, mm3
    movq    [edx + 50H], mm1
    punpckhdq mm4, mm5
    punpckldq mm2, mm5
    movq    [edx + 70H], mm4
    movq    [edx + 60H], mm2
    movq    mm2, [edx + 30H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 50H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 10H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 70H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 20H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 60H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 10H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 20H], mm6
    movq    mm2, mm0
    movq    mm6, [edx]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 40H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 10H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    paddw   mm2, [eax + 38H]
    paddw   mm1, mm1
    paddw   mm1, mm2
    psraw   mm2, 4
    psubw   mm4, mm7
    psraw   mm1, 4
    movq    mm3, [edx + 20H]
    paddw   mm7, mm7
    movq    [edx + 20H], mm2
    paddw   mm7, mm4
    movq    [edx + 10H], mm1
    psubw   mm4, mm3
    paddw   mm4, [eax + 38H]
    paddw   mm3, mm3
    paddw   mm3, mm4
    psraw   mm4, 4
    psubw   mm6, mm5
    psraw   mm3, 4
    paddw   mm6, [eax + 38H]
    paddw   mm5, mm5
    paddw   mm5, mm6
    psraw   mm6, 4
    movq    [edx + 40H], mm4
    psraw   mm5, 4
    movq    [edx + 30H], mm3
    psubw   mm7, mm0
    paddw   mm7, [eax + 38H]
    paddw   mm0, mm0
    paddw   mm0, mm7
    psraw   mm7, 4
    movq    [edx + 60H], mm6
    psraw   mm0, 4
    movq    [edx + 50H], mm5
    movq    [edx + 70H], mm7
    movq    [edx], mm0
    movq    mm2, [edx + 38H]
    movq    mm6, [eax + 10H]
    movq    mm4, mm2
    movq    mm7, [edx + 58H]
    pmulhw  mm4, mm6
    movq    mm1, [eax + 20H]
    pmulhw  mm6, mm7
    movq    mm5, mm1
    pmulhw  mm1, mm2
    movq    mm3, [edx + 18H]
    pmulhw  mm5, mm7
    movq    mm0, [eax]
    paddw   mm4, mm2
    paddw   mm6, mm7
    paddw   mm2, mm1
    movq    mm1, [edx + 78H]
    paddw   mm7, mm5
    movq    mm5, mm0
    pmulhw  mm0, mm3
    paddw   mm4, mm7
    pmulhw  mm5, mm1
    movq    mm7, [eax + 30H]
    psubw   mm6, mm2
    paddw   mm0, mm3
    pmulhw  mm3, mm7
    movq    mm2, [edx + 28H]
    pmulhw  mm7, mm1
    paddw   mm5, mm1
    movq    mm1, mm2
    pmulhw  mm2, [eax + 08H]
    psubw   mm3, mm5
    movq    mm5, [edx + 68H]
    paddw   mm0, mm7
    movq    mm7, mm5
    psubw   mm0, mm4
    pmulhw  mm5, [eax + 08H]
    paddw   mm2, mm1
    pmulhw  mm1, [eax + 28H]
    paddw   mm4, mm4
    paddw   mm4, mm0
    psubw   mm3, mm6
    paddw   mm5, mm7
    paddw   mm6, mm6
    pmulhw  mm7, [eax + 28H]
    paddw   mm6, mm3
    movq    [edx + 18H], mm4
    psubw   mm1, mm5
    movq    mm4, [eax + 18H]
    movq    mm5, mm3
    pmulhw  mm3, mm4
    paddw   mm7, mm2
    movq    [edx + 28H], mm6
    movq    mm2, mm0
    movq    mm6, [edx + 08H]
    pmulhw  mm0, mm4
    paddw   mm5, mm3
    movq    mm3, [edx + 48H]
    psubw   mm5, mm1
    paddw   mm2, mm0
    psubw   mm6, mm3
    movq    mm0, mm6
    pmulhw  mm6, mm4
    paddw   mm3, mm3
    paddw   mm1, mm1
    paddw   mm3, mm0
    paddw   mm1, mm5
    pmulhw  mm4, mm3
    paddw   mm6, mm0
    psubw   mm6, mm2
    paddw   mm2, mm2
    movq    mm0, [edx + 18H]
    paddw   mm2, mm6
    paddw   mm4, mm3
    psubw   mm2, mm1
    paddw   mm2, [eax + 38H]
    paddw   mm1, mm1
    paddw   mm1, mm2
    psraw   mm2, 4
    psubw   mm4, mm7
    psraw   mm1, 4
    movq    mm3, [edx + 28H]
    paddw   mm7, mm7
    movq    [edx + 28H], mm2
    paddw   mm7, mm4
    movq    [edx + 18H], mm1
    psubw   mm4, mm3
    paddw   mm4, [eax + 38H]
    paddw   mm3, mm3
    paddw   mm3, mm4
    psraw   mm4, 4
    psubw   mm6, mm5
    psraw   mm3, 4
    paddw   mm6, [eax + 38H]
    paddw   mm5, mm5
    paddw   mm5, mm6
    psraw   mm6, 4
    movq    [edx + 48H], mm4
    psraw   mm5, 4
    movq    [edx + 38H], mm3
    psubw   mm7, mm0
    paddw   mm7, [eax + 38H]
    paddw   mm0, mm0
    paddw   mm0, mm7
    psraw   mm7, 4
    movq    [edx + 68H], mm6
    psraw   mm0, 4
    movq    [edx + 58H], mm5
    movq    [edx + 78H], mm7
    movq    [edx + 08H], mm0
    /* emms  */
  }
}

#endif
