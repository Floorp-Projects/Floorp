/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_RECON_H
#define __INC_RECON_H

#define prototype_copy_block(sym) \
    void sym(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch)

#define prototype_recon_block(sym) \
    void sym(unsigned char *pred, short *diff, unsigned char *dst, int pitch);

#if ARCH_X86 || ARCH_X86_64
#include "x86/recon_x86.h"
#endif

#if ARCH_ARM
#include "arm/recon_arm.h"
#endif

#ifndef vp8_recon_copy16x16
#define vp8_recon_copy16x16 vp8_copy_mem16x16_c
#endif
extern prototype_copy_block(vp8_recon_copy16x16);

#ifndef vp8_recon_copy8x8
#define vp8_recon_copy8x8 vp8_copy_mem8x8_c
#endif
extern prototype_copy_block(vp8_recon_copy8x8);

#ifndef vp8_recon_copy8x4
#define vp8_recon_copy8x4 vp8_copy_mem8x4_c
#endif
extern prototype_copy_block(vp8_recon_copy8x4);

#ifndef vp8_recon_recon
#define vp8_recon_recon vp8_recon_b_c
#endif
extern prototype_recon_block(vp8_recon_recon);

#ifndef vp8_recon_recon2
#define vp8_recon_recon2 vp8_recon2b_c
#endif
extern prototype_recon_block(vp8_recon_recon2);

#ifndef vp8_recon_recon4
#define vp8_recon_recon4 vp8_recon4b_c
#endif
extern prototype_recon_block(vp8_recon_recon4);

typedef prototype_copy_block((*vp8_copy_block_fn_t));
typedef prototype_recon_block((*vp8_recon_fn_t));
typedef struct
{
    vp8_copy_block_fn_t  copy16x16;
    vp8_copy_block_fn_t  copy8x8;
    vp8_copy_block_fn_t  copy8x4;
    vp8_recon_fn_t       recon;
    vp8_recon_fn_t       recon2;
    vp8_recon_fn_t       recon4;
} vp8_recon_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define RECON_INVOKE(ctx,fn) (ctx)->fn
#else
#define RECON_INVOKE(ctx,fn) vp8_recon_##fn
#endif

#include "blockd.h"
void vp8_recon16x16mby(const vp8_recon_rtcd_vtable_t *rtcd, MACROBLOCKD *x);
void vp8_recon16x16mb(const vp8_recon_rtcd_vtable_t *rtcd, MACROBLOCKD *x);
void vp8_recon_intra4x4mb(const vp8_recon_rtcd_vtable_t *rtcd, MACROBLOCKD *x);
void vp8_recon_intra_mbuv(const vp8_recon_rtcd_vtable_t *rtcd, MACROBLOCKD *x);
#endif
