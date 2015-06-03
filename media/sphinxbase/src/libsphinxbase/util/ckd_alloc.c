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
 * ckd_alloc.c -- Memory allocation package.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 *
 * HISTORY
 * $Log: ckd_alloc.c,v $
 * Revision 1.6  2005/06/22 02:59:25  arthchan2003
 * Added  keyword
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 *
 * 19-Jun-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Removed file,line arguments from free functions.
 * 		Removed debugging stuff.
 *
 * 01-Jan-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created.
 */


/*********************************************************************
 *
 * $Header: /cvsroot/cmusphinx/sphinx3/src/libutil/ckd_alloc.c,v 1.6 2005/06/22 02:59:25 arthchan2003 Exp $
 *
 * Carnegie Mellon ARPA Speech Group
 *
 * Copyright (c) 1994 Carnegie Mellon University.
 * All rights reserved.
 *
 *********************************************************************
 *
 * file: ckd_alloc.c
 *
 * traceability:
 *
 * description:
 *
 * author:
 *
 *********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#ifdef _MSC_VER
#pragma warning (disable: 4996)
#endif

#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/err.h"

/**
 * Target for longjmp() on failure.
 *
 * FIXME: This should be in thread-local storage.
 */
static jmp_buf *ckd_target;
static int jmp_abort;

jmp_buf *
ckd_set_jump(jmp_buf *env, int abort)
{
    jmp_buf *old;

    if (abort)
        jmp_abort = 1;

    old = ckd_target;
    ckd_target = env;
    return old;
}

void
ckd_fail(char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (jmp_abort)
        /* abort() doesn't exist in Windows CE */
        #if defined(_WIN32_WCE)
        exit(-1);
        #else
        abort();
        #endif
    else if (ckd_target)
        longjmp(*ckd_target, 1);
    else
        exit(-1);
}

void *
__ckd_calloc__(size_t n_elem, size_t elem_size,
               const char *caller_file, int caller_line)
{
    void *mem;

#if defined(__ADSPBLACKFIN__) && !defined(__linux__)
    if ((mem = heap_calloc(heap_lookup(1),n_elem, elem_size)) == NULL)
    	if ((mem = heap_calloc(heap_lookup(0),n_elem, elem_size)) == NULL)
    	{
        	ckd_fail("calloc(%d,%d) failed from %s(%d), free space: %d\n", n_elem,
                elem_size, caller_file, caller_line,space_unused());
    	}
#else
    if ((mem = calloc(n_elem, elem_size)) == NULL) {
        ckd_fail("calloc(%d,%d) failed from %s(%d)\n", n_elem,
                elem_size, caller_file, caller_line);
	}
#endif


    return mem;
}


void *
__ckd_malloc__(size_t size, const char *caller_file, int caller_line)
{
    void *mem;

#if defined(__ADSPBLACKFIN__) && !defined(__linux__)
    if ((mem = heap_malloc(heap_lookup(0),size)) == NULL)
       	if ((mem = heap_malloc(heap_lookup(1),size)) == NULL)
#else
    if ((mem = malloc(size)) == NULL)
#endif
	        ckd_fail("malloc(%d) failed from %s(%d)\n", size,
                caller_file, caller_line);

    return mem;
}


void *
__ckd_realloc__(void *ptr, size_t new_size,
                const char *caller_file, int caller_line)
{
    void *mem;
#if defined(__ADSPBLACKFIN__) && !defined(__linux__)
    if ((mem = heap_realloc(heap_lookup(0),ptr, new_size)) == NULL) {
#else
    if ((mem = realloc(ptr, new_size)) == NULL) {
#endif
        ckd_fail("malloc(%d) failed from %s(%d)\n", new_size,
                caller_file, caller_line);
    }

    return mem;
}


char *
__ckd_salloc__(const char *orig, const char *caller_file,
               int caller_line)
{
    size_t len;
    char *buf;

    if (!orig)
        return NULL;

    len = strlen(orig) + 1;
    buf = (char *) __ckd_malloc__(len, caller_file, caller_line);

    strcpy(buf, orig);
    return (buf);
}


void *
__ckd_calloc_2d__(size_t d1, size_t d2, size_t elemsize,
                  const char *caller_file, int caller_line)
{
    char **ref, *mem;
    size_t i, offset;

    mem =
        (char *) __ckd_calloc__(d1 * d2, elemsize, caller_file,
                                caller_line);
    ref =
        (char **) __ckd_malloc__(d1 * sizeof(void *), caller_file,
                                 caller_line);

    for (i = 0, offset = 0; i < d1; i++, offset += d2 * elemsize)
        ref[i] = mem + offset;

    return ref;
}


void
ckd_free(void *ptr)
{
#if defined(__ADSPBLACKFIN__) && !defined(__linux__)
    if (ptr)
        heap_free(0,ptr);
#else
    free(ptr);
#endif
}

void
ckd_free_2d(void *tmpptr)
{
    void **ptr = (void **)tmpptr;
    if (ptr)
        ckd_free(ptr[0]);
    ckd_free(ptr);
}


void *
__ckd_calloc_3d__(size_t d1, size_t d2, size_t d3, size_t elemsize,
                  const char *caller_file, int caller_line)
{
    char ***ref1, **ref2, *mem;
    size_t i, j, offset;

    mem =
        (char *) __ckd_calloc__(d1 * d2 * d3, elemsize, caller_file,
                                caller_line);
    ref1 =
        (char ***) __ckd_malloc__(d1 * sizeof(void **), caller_file,
                                  caller_line);
    ref2 =
        (char **) __ckd_malloc__(d1 * d2 * sizeof(void *), caller_file,
                                 caller_line);

    for (i = 0, offset = 0; i < d1; i++, offset += d2)
        ref1[i] = ref2 + offset;

    offset = 0;
    for (i = 0; i < d1; i++) {
        for (j = 0; j < d2; j++) {
            ref1[i][j] = mem + offset;
            offset += d3 * elemsize;
        }
    }

    return ref1;
}


void
ckd_free_3d(void *inptr)
{
    void ***ptr = (void ***)inptr;

    if (ptr && ptr[0])
        ckd_free(ptr[0][0]);
    if (ptr)
        ckd_free(ptr[0]);
    ckd_free(ptr);
}


void ****
__ckd_calloc_4d__(size_t d1,
		  size_t d2,
		  size_t d3,
		  size_t d4,
		  size_t elem_size,
		  char *file,
		  int line)
{
    void *store;
    void **tmp1;
    void ***tmp2;
    void ****out;
    size_t i, j;

    store = calloc(d1 * d2 * d3 * d4, elem_size);
    if (store == NULL) {
	E_FATAL("ckd_calloc_4d failed for caller at %s(%d) at %s(%d)\n",
		file, line, __FILE__, __LINE__);
    }

    tmp1 = calloc(d1 * d2 * d3, sizeof(void *));
    if (tmp1 == NULL) {
	E_FATAL("ckd_calloc_4d failed for caller at %s(%d) at %s(%d)\n",
		file, line, __FILE__, __LINE__);
    }

    tmp2 = ckd_calloc(d1 * d2, sizeof(void **));
    if (tmp2 == NULL) {
	E_FATAL("ckd_calloc_4d failed for caller at %s(%d) at %s(%d)\n",
		file, line, __FILE__, __LINE__);
    }

    out = ckd_calloc(d1, sizeof(void ***));
    if (out == NULL) {
	E_FATAL("ckd_calloc_4d failed for caller at %s(%d) at %s(%d)\n",
		file, line, __FILE__, __LINE__);
    }

    for (i = 0, j = 0; i < d1*d2*d3; i++, j += d4) {
	tmp1[i] = &((char *)store)[j*elem_size];
    }

    for (i = 0, j = 0; i < d1*d2; i++, j += d3) {
	tmp2[i] = &tmp1[j];
    }

    for (i = 0, j = 0; i < d1; i++, j += d2) {
	out[i] = &tmp2[j];
    }

    return out;
}

void
ckd_free_4d(void *inptr)
{
    void ****ptr = (void ****)inptr;
    if (ptr == NULL)
	return;
    /* free the underlying store */
    ckd_free(ptr[0][0][0]);

    /* free the access overhead */
    ckd_free(ptr[0][0]);
    ckd_free(ptr[0]);
    ckd_free(ptr);
}

/* Layers a 3d array access structure over a preallocated storage area */
void *
__ckd_alloc_3d_ptr(size_t d1,
		   size_t d2,
		   size_t d3,
		   void *store,
		   size_t elem_size,
		   char *file,
		   int line)
{
    void **tmp1;
    void ***out;
    size_t i, j;

    tmp1 = __ckd_calloc__(d1 * d2, sizeof(void *), file, line);

    out  = __ckd_calloc__(d1, sizeof(void **), file, line);

    for (i = 0, j = 0; i < d1*d2; i++, j += d3) {
	tmp1[i] = &((char *)store)[j*elem_size];
    }

    for (i = 0, j = 0; i < d1; i++, j += d2) {
	out[i] = &tmp1[j];
    }

    return out;
}

void *
__ckd_alloc_2d_ptr(size_t d1,
		   size_t d2,
		   void *store,
		   size_t elem_size,
		   char *file,
		   int line)
{
    void **out;
    size_t i, j;

    out = __ckd_calloc__(d1, sizeof(void *), file, line);

    for (i = 0, j = 0; i < d1; i++, j += d2) {
	out[i] = &((char *)store)[j*elem_size];
    }

    return out;
}

/* vim: set ts=4 sw=4: */
