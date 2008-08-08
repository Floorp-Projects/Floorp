/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************
 function:
    last mod: $Id: cpu.h 13884 2007-09-22 08:38:10Z giles $

 ********************************************************************/

#if !defined(_x86_cpu_H)
# define _x86_cpu_H (1)
#include "internal.h"

#define OC_CPU_X86_MMX    (1<<0)
#define OC_CPU_X86_3DNOW  (1<<1)
#define OC_CPU_X86_3DNOWEXT (1<<2)
#define OC_CPU_X86_MMXEXT (1<<3)
#define OC_CPU_X86_SSE    (1<<4)
#define OC_CPU_X86_SSE2   (1<<5)

ogg_uint32_t oc_cpu_flags_get(void);

#endif
