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
 * ckd_alloc.h -- Memory allocation package.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 *
 * HISTORY
 * $Log: ckd_alloc.h,v $
 * Revision 1.10  2005/06/22 02:59:25  arthchan2003
 * Added  keyword
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 *
 * 19-Jun-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Removed file,line arguments from free functions.
 *
 * 01-Jan-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created.
 */


/*********************************************************************
 *
 * $Header: /cvsroot/cmusphinx/sphinx3/src/libutil/ckd_alloc.h,v 1.10 2005/06/22 02:59:25 arthchan2003 Exp $
 *
 * Carnegie Mellon ARPA Speech Group
 *
 * Copyright (c) 1994 Carnegie Mellon University.
 * All rights reserved.
 *
 *********************************************************************
 *
 * file: ckd_alloc.h
 *
 * traceability:
 *
 * description:
 *
 * author:
 *
 *********************************************************************/


#ifndef _LIBUTIL_CKD_ALLOC_H_
#define _LIBUTIL_CKD_ALLOC_H_

#include <stdlib.h>
#include <setjmp.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

/** \file ckd_alloc.h
 *\brief Sphinx's memory allocation/deallocation routines.
 *
 *Implementation of efficient memory allocation deallocation for
 *multiple dimensional arrays.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * Control behaviour of the program when allocation fails.
 *
 * Although your program is probably toast when memory allocation
 * fails, it is also probably a good idea to be able to catch these
 * errors and alert the user in some way.  Either that, or you might
 * want the program to call abort() so that you can debug the failed
 * code.  This function allows you to control that behaviour.
 *
 * @param env Pointer to a <code>jmp_buf</code> initialized with
 * setjmp(), or NULL to remove a previously set jump target.
 * @param abort If non-zero, the program will call abort() when
 * allocation fails rather than exiting or calling longjmp().
 * @return Pointer to a previously set <code>jmp_buf</code>, if any.
 */
jmp_buf *ckd_set_jump(jmp_buf *env, int abort);

/**
 * Fail (with a message) according to behaviour specified by ckd_set_jump().
 */
void ckd_fail(char *format, ...);

/*
 * The following functions are similar to the malloc family, except
 * that they have two additional parameters, caller_file and
 * caller_line, for error reporting.  All functions print a diagnostic
 * message if any error occurs, with any other behaviour determined by
 * ckd_set_jump(), above.
 */

SPHINXBASE_EXPORT
void *__ckd_calloc__(size_t n_elem, size_t elem_size,
		     const char *caller_file, int caller_line);

SPHINXBASE_EXPORT
void *__ckd_malloc__(size_t size,
		     const char *caller_file, int caller_line);

SPHINXBASE_EXPORT
void *__ckd_realloc__(void *ptr, size_t new_size,
		      const char *caller_file, int caller_line);

/**
 * Like strdup, except that if an error occurs it prints a diagnostic message and
 * exits. If origin in NULL the function also returns NULL.
 */
SPHINXBASE_EXPORT
char *__ckd_salloc__(const char *origstr,
		     const char *caller_file, int caller_line);

/**
 * Allocate a 2-D array and return ptr to it (ie, ptr to vector of ptrs).
 * The data area is allocated in one block so it can also be treated as a 1-D array.
 */
SPHINXBASE_EXPORT
void *__ckd_calloc_2d__(size_t d1, size_t d2,	/* In: #elements in the 2 dimensions */
                        size_t elemsize,	/* In: Size (#bytes) of each element */
                        const char *caller_file, int caller_line);	/* In */

/**
 * Allocate a 3-D array and return ptr to it.
 * The data area is allocated in one block so it can also be treated as a 1-D array.
 */
SPHINXBASE_EXPORT
void *__ckd_calloc_3d__(size_t d1, size_t d2, size_t d3,	/* In: #elems in the dims */
                        size_t elemsize,		/* In: Size (#bytes) per element */
                        const char *caller_file, int caller_line);	/* In */

/**
 * Allocate a 34D array and return ptr to it.
 * The data area is allocated in one block so it can also be treated as a 1-D array.
 */
SPHINXBASE_EXPORT
void ****__ckd_calloc_4d__(size_t d1,
			   size_t d2,
			   size_t d3,
			   size_t d4,
			   size_t elem_size,
			   char *caller_file,
			   int caller_line);

/**
 * Overlay a 3-D array over a previously allocated storage area.
 **/
SPHINXBASE_EXPORT
void * __ckd_alloc_3d_ptr(size_t d1,
                          size_t d2,
                          size_t d3,
                          void *store,
                          size_t elem_size,
                          char *caller_file,
                          int caller_line);

/**
 * Overlay a s-D array over a previously allocated storage area.
 **/
SPHINXBASE_EXPORT
void *__ckd_alloc_2d_ptr(size_t d1,
                         size_t d2,
                         void *store,
                         size_t elem_size,
                         char *caller_file,
                         int caller_line);

/**
 * Test and free a 1-D array
 */
SPHINXBASE_EXPORT
void ckd_free(void *ptr);

/**
 * Free a 2-D array (ptr) previously allocated by ckd_calloc_2d
 */
SPHINXBASE_EXPORT
void ckd_free_2d(void *ptr);

/**
 * Free a 3-D array (ptr) previously allocated by ckd_calloc_3d
 */
SPHINXBASE_EXPORT
void ckd_free_3d(void *ptr);

/**
 * Free a 4-D array (ptr) previously allocated by ckd_calloc_4d
 */
SPHINXBASE_EXPORT
void ckd_free_4d(void *ptr);

/**
 * Macros to simplify the use of above functions.
 * One should use these, rather than target functions directly.
 */

/**
 * Macro for __ckd_calloc__
 */
#define ckd_calloc(n,sz)	__ckd_calloc__((n),(sz),__FILE__,__LINE__)

/**
 * Macro for __ckd_malloc__
 */
#define ckd_malloc(sz)		__ckd_malloc__((sz),__FILE__,__LINE__)

/**
 * Macro for __ckd_realloc__
 */
#define ckd_realloc(ptr,sz)	__ckd_realloc__(ptr,(sz),__FILE__,__LINE__)

/**
 * Macro for __ckd_salloc__
 */

#define ckd_salloc(ptr)		__ckd_salloc__(ptr,__FILE__,__LINE__)

/**
 * Macro for __ckd_calloc_2d__
 */

#define ckd_calloc_2d(d1,d2,sz)	__ckd_calloc_2d__((d1),(d2),(sz),__FILE__,__LINE__)

/**
 * Macro for __ckd_calloc_3d__
 */

#define ckd_calloc_3d(d1,d2,d3,sz) __ckd_calloc_3d__((d1),(d2),(d3),(sz),__FILE__,__LINE__)

/**
 * Macro for __ckd_calloc_4d__
 */
#define ckd_calloc_4d(d1, d2, d3, d4, s)  __ckd_calloc_4d__((d1), (d2), (d3), (d4), (s), __FILE__, __LINE__)

/**
 * Macro for __ckd_alloc_2d_ptr__
 */

#define ckd_alloc_2d_ptr(d1, d2, bf, sz)    __ckd_alloc_2d_ptr((d1), (d2), (bf), (sz), __FILE__, __LINE__)

/**
 * Free only the pointer arrays allocated with ckd_alloc_2d_ptr().
 */
#define ckd_free_2d_ptr(bf) ckd_free(bf)

/**
 * Macro for __ckd_alloc_3d_ptr__
 */

#define ckd_alloc_3d_ptr(d1, d2, d3, bf, sz) __ckd_alloc_3d_ptr((d1), (d2), (d3), (bf), (sz), __FILE__, __LINE__)

/**
 * Free only the pointer arrays allocated with ckd_alloc_3d_ptr().
 */
#define ckd_free_3d_ptr(bf) ckd_free_2d(bf)


#ifdef __cplusplus
}
#endif

#endif
