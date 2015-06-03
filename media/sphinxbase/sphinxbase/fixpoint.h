/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2005 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ==================================================================== */

/* Fixed-point arithmetic macros.
 *
 * Author: David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#ifndef _FIXPOINT_H_
#define _FIXPOINT_H_

#include <limits.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

#ifndef DEFAULT_RADIX
#define DEFAULT_RADIX 12
#endif

/** Fixed-point computation type. */
typedef int32 fixed32;

/** Convert floating point to fixed point. */
#define FLOAT2FIX_ANY(x,radix) \
	(((x)<0.0) ? \
	((fixed32)((x)*(float32)(1<<(radix)) - 0.5)) \
	: ((fixed32)((x)*(float32)(1<<(radix)) + 0.5)))
#define FLOAT2FIX(x) FLOAT2FIX_ANY(x,DEFAULT_RADIX)
/** Convert fixed point to floating point. */
#define FIX2FLOAT_ANY(x,radix) ((float32)(x)/(1<<(radix)))
#define FIX2FLOAT(x) FIX2FLOAT_ANY(x,DEFAULT_RADIX)

/**
 * Multiply two fixed point numbers with an arbitrary radix point.
 *
 * A veritable multiplicity of implementations exist, starting with
 * the fastest ones...
 */

#if defined(__arm__) && !defined(__thumb__)
/* 
 * This works on most modern ARMs but *only* in ARM mode (for obvious
 * reasons), so don't use it in Thumb mode (but why are you building
 * signal processing code in Thumb mode?!)
 */
#define FIXMUL(a,b) FIXMUL_ANY(a,b,DEFAULT_RADIX)
#define FIXMUL_ANY(a,b,r) ({				\
      int cl, ch, _a = a, _b = b;			\
      __asm__ ("smull %0, %1, %2, %3\n"			\
	   "mov %0, %0, lsr %4\n"			\
	   "orr %0, %0, %1, lsl %5\n"			\
	   : "=&r" (cl), "=&r" (ch)			\
	   : "r" (_a), "r" (_b), "i" (r), "i" (32-(r)));\
      cl; })

#elif defined(_MSC_VER) || (defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 8) 
/* Standard systems*/
#define FIXMUL(a,b) FIXMUL_ANY(a,b,DEFAULT_RADIX)
#define FIXMUL_ANY(a,b,radix) ((fixed32)(((int64)(a)*(b))>>(radix)))

#else
/* Most general case where 'long long' doesn't exist or is slow. */
#define FIXMUL(a,b) FIXMUL_ANY(a,b,DEFAULT_RADIX)
#define FIXMUL_ANY(a,b,radix) ({ \
	int32 _ah, _bh; \
	uint32 _al, _bl, _t, c; \
	_ah = ((int32)(a)) >> 16; \
	_bh = ((int32)(b)) >> 16; \
	_al = ((uint32)(a)) & 0xffff; \
	_bl = ((uint32)(b)) & 0xffff; \
	_t = _ah * _bl + _al * _bh; \
	c = (fixed32)(((_al * _bl) >> (radix)) \
		      + ((_ah * _bh) << (32 - (radix))) \
		      + ((radix) > 16 ? (_t >> (radix - 16)) : (_t << (16 - radix)))); \
	c;})
#endif

/* Various fixed-point logarithmic functions that we need. */
/** Minimum value representable in log format. */
#define MIN_FIXLOG -2829416  /* log(1e-300) * (1<<DEFAULT_RADIX) */
#define MIN_FIXLOG2 -4081985 /* log2(1e-300) * (1<<DEFAULT_RADIX) */
/** Fixed-point representation of log(2) */
#define FIXLN_2		((fixed32)(0.693147180559945 * (1<<DEFAULT_RADIX)))
/** Take natural logarithm of a fixedpoint number. */
#define FIXLN(x) (fixlog(x) - (FIXLN_2 * DEFAULT_RADIX))

/**
 * Take natural logarithm of an integer, yielding a fixedpoint number
 * with DEFAULT_RADIX as radix point.
 */
int32 fixlog(uint32 x);
/**
 * Take base-2 logarithm of an integer, yielding a fixedpoint number
 * with DEFAULT_RADIX as radix point.
 */
int32 fixlog2(uint32 x);

#ifdef __cplusplus
}
#endif


#endif /* _FIXPOINT_H_ */
