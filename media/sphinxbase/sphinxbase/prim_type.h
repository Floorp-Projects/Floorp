/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2004 Carnegie Mellon University.  All rights
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
 * ====================================================================
 *
 */
/*
 * prim_type.h -- Primitive types; more machine-independent.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: prim_type.h,v $
 * Revision 1.12  2005/10/05 00:31:14  dhdfu
 * Make int8 be explicitly signed (signedness of 'char' is
 * architecture-dependent).  Then make a bunch of things use uint8 where
 * signedness is unimportant, because on the architecture where 'char' is
 * unsigned, it is that way for a reason (signed chars are slower).
 *
 * Revision 1.11  2005/06/22 03:10:23  arthchan2003
 * Added  keyword.
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 12-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Added arraysize_t, point_t, fpoint_t.
 * 
 * 01-Feb-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Added anytype_t.
 * 
 * 08-31-95	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Created.
 */


#ifndef _LIBUTIL_PRIM_TYPE_H_
#define _LIBUTIL_PRIM_TYPE_H_

/**
 * @file prim_type.h
 * @brief Basic type definitions used in Sphinx. 
 */
 
#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* Fool Emacs into not indenting things. */
#endif

#include <sphinx_config.h>

/* Define some things for VisualDSP++ */
#if defined(__ADSPBLACKFIN__) && !defined(__GNUC__)
# ifndef HAVE_LONG_LONG
#  define HAVE_LONG_LONG
# endif
# ifndef ssize_t
typedef signed int ssize_t;
# endif
# define SIZEOF_LONG_LONG 8
# define __BIGSTACKVARIABLE__ static
#else /* Not VisualDSP++ */
# define __BIGSTACKVARIABLE__
#endif

/**
 * Union of basic types.
 */
typedef union anytype_s {
    void *ptr;
    long i;
    unsigned long ui;
    double fl;
} anytype_t;

/*
 * Assume P64 or LP64.  If you need to port this to a DSP, let us know.
 */
typedef int		int32;
typedef short		int16;
typedef signed char	int8;
typedef unsigned int	uint32;
typedef unsigned short	uint16;
typedef unsigned char	uint8;
typedef float		float32;
typedef double		float64;
#if defined(_MSC_VER)
typedef __int64	         int64;
typedef unsigned __int64 uint64;
#elif defined(HAVE_LONG_LONG) && (SIZEOF_LONG_LONG == 8)
typedef long long	   int64;
typedef unsigned long long uint64;
#else /* !HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8 */
typedef double          int64;
typedef double          uint64;
#endif /* !HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8 */

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL (void *)0
#endif

/* These really ought to come from <limits.h>, but not everybody has that. */
/* Useful constants */
#define MAX_INT32		((int32) 0x7fffffff)
#define MAX_INT16		((int16) 0x00007fff)
#define MAX_INT8		((int8)  0x0000007f)

#define MAX_NEG_INT32		((int32) 0x80000000)
#define MAX_NEG_INT16		((int16) 0xffff8000)
#define MAX_NEG_INT8		((int8)  0xffffff80)

#define MAX_UINT32		((uint32) 0xffffffff)
#define MAX_UINT16		((uint16) 0x0000ffff)
#define MAX_UINT8		((uint8)  0x000000ff)

/* The following are approximate; IEEE floating point standards might quibble! */
#define MAX_POS_FLOAT32		3.4e+38f
#define MIN_POS_FLOAT32		1.2e-38f	/* But not 0 */
#define MAX_POS_FLOAT64		1.8e+307
#define MIN_POS_FLOAT64		2.2e-308

#define MAX_IEEE_NORM_POS_FLOAT32        3.4e+38f
#define MIN_IEEE_NORM_POS_FLOAT32        1.2e-38f
#define MIN_IEEE_NORM_NEG_FLOAT32       -3.4e+38f
#define MAX_IEEE_NORM_POS_FLOAT64        1.8e+307
#define MIN_IEEE_NORM_POS_FLOAT64        2.2e-308
#define MIN_IEEE_NORM_NEG_FLOAT64       -1.8e+307

/* Will the following really work?? */
#define MIN_NEG_FLOAT32		((float32) (-MIN_POS_FLOAT32))
#define MIN_NEG_FLOAT64		((float64) (-MIN_POS_FLOAT64))


#ifdef __cplusplus
}
#endif

#endif
