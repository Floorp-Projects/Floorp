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

#ifndef __LISTELEM_ALLOC_H__
#define __LISTELEM_ALLOC_H__

/** @file listelem_alloc.h
 * @brief Fast memory allocator for uniformly sized objects
 * @author M K Ravishankar <rkm@cs.cmu.edu>
 */
#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

#include <stdlib.h>
#ifdef S60
#include <types.h>
#endif

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

/**
 * List element allocator object.
 */
typedef struct listelem_alloc_s listelem_alloc_t;

/**
 * Initialize and return a list element allocator.
 */
SPHINXBASE_EXPORT
listelem_alloc_t * listelem_alloc_init(size_t elemsize);

/**
 * Finalize and release all memory associated with a list element allocator.
 */
SPHINXBASE_EXPORT
void listelem_alloc_free(listelem_alloc_t *le);


SPHINXBASE_EXPORT
void *__listelem_malloc__(listelem_alloc_t *le, char *file, int line);

/** 
 * Allocate a list element and return pointer to it.
 */
#define listelem_malloc(le)	__listelem_malloc__((le),__FILE__,__LINE__)

SPHINXBASE_EXPORT
void *__listelem_malloc_id__(listelem_alloc_t *le, char *file, int line,
                             int32 *out_id);

/**
 * Allocate a list element, returning a unique identifier.
 */
#define listelem_malloc_id(le, oid)	__listelem_malloc_id__((le),__FILE__,__LINE__,(oid))

/**
 * Retrieve a list element by its identifier.
 */
SPHINXBASE_EXPORT
void *listelem_get_item(listelem_alloc_t *le, int32 id);

/**
 * Free list element of given size 
 */
SPHINXBASE_EXPORT
void __listelem_free__(listelem_alloc_t *le, void *elem, char *file, int line);

/** 
 * Macro of __listelem_free__
 */
#define listelem_free(le,el)	__listelem_free__((le),(el),__FILE__,__LINE__)

/**
   Print number of allocation, numer of free operation stats 
*/
SPHINXBASE_EXPORT
void listelem_stats(listelem_alloc_t *le);


#ifdef __cplusplus
}
#endif

#endif
