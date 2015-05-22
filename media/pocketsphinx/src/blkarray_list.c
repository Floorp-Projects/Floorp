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
 * blkarray_list.c -- block array-based list structure.
 * 
 * HISTORY
 * 
 * 18-Feb-2004	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Started.
 */

/* System headers. */
#include <assert.h>

/* SphinxBase headers. */
#include <sphinxbase/prim_type.h>
#include <sphinxbase/err.h>
#include <sphinxbase/ckd_alloc.h>

/* Local headers. */
#include "blkarray_list.h"


#define BLKARRAY_DEFAULT_MAXBLKS	16380
#define BLKARRAY_DEFAULT_BLKSIZE	16380


blkarray_list_t *
_blkarray_list_init(int32 maxblks, int32 blksize)
{
    blkarray_list_t *bl;

    if ((maxblks <= 0) || (blksize <= 0)) {
        E_ERROR("Cannot allocate %dx%d blkarray\n", maxblks, blksize);
        return NULL;
    }

    bl = (blkarray_list_t *) ckd_calloc(1, sizeof(blkarray_list_t));
    bl->ptr = (void ***) ckd_calloc(maxblks, sizeof(void **));
    bl->maxblks = maxblks;
    bl->blksize = blksize;
    bl->n_valid = 0;
    bl->cur_row = -1;           /* No row is allocated (dummy) */
    bl->cur_row_free = blksize; /* The dummy row is full */

    return bl;
}


blkarray_list_t *
blkarray_list_init(void)
{
    return _blkarray_list_init(BLKARRAY_DEFAULT_MAXBLKS,
                               BLKARRAY_DEFAULT_BLKSIZE);
}

void
blkarray_list_free(blkarray_list_t *bl)
{
    blkarray_list_reset(bl);
    ckd_free(bl->ptr);
    ckd_free(bl);
}


int32
blkarray_list_append(blkarray_list_t * bl, void *data)
{
    int32 id;

    assert(bl);

    if (bl->cur_row_free >= bl->blksize) {
        /* Previous row is filled; need to allocate a new row */
        bl->cur_row++;

        if (bl->cur_row >= bl->maxblks) {
            E_ERROR("Block array (%dx%d) exhausted\n",
                    bl->maxblks, bl->blksize);
            bl->cur_row--;
            return -1;
        }

        /* Allocate the new row */
        assert(bl->ptr[bl->cur_row] == NULL);
        bl->ptr[bl->cur_row] = (void **) ckd_malloc(bl->blksize *
                                                    sizeof(void *));

        bl->cur_row_free = 0;
    }

    bl->ptr[bl->cur_row][bl->cur_row_free] = data;
    (bl->cur_row_free)++;

    id = (bl->n_valid)++;
    assert(id >= 0);

    return id;
}


void
blkarray_list_reset(blkarray_list_t * bl)
{
    int32 i, j;

    /* Free all the allocated elements as well as the blocks */
    for (i = 0; i < bl->cur_row; i++) {
        for (j = 0; j < bl->blksize; j++)
            ckd_free(bl->ptr[i][j]);

        ckd_free(bl->ptr[i]);
        bl->ptr[i] = NULL;
    }
    if (i == bl->cur_row) {     /* NEED THIS! (in case cur_row < 0) */
        for (j = 0; j < bl->cur_row_free; j++)
            ckd_free(bl->ptr[i][j]);

        ckd_free(bl->ptr[i]);
        bl->ptr[i] = NULL;
    }

    bl->n_valid = 0;
    bl->cur_row = -1;
    bl->cur_row_free = bl->blksize;
}

void *
blkarray_list_get(blkarray_list_t *list, int32 n)
{
    int32 r, c;

    if (n >= blkarray_list_n_valid(list))
        return NULL;

    r = n / blkarray_list_blksize(list);
    c = n - (r * blkarray_list_blksize(list));

    return blkarray_list_ptr(list, r, c);
}
