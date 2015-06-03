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
 * glist.h -- Module for maintaining a generic, linear linked-list structure.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: glist.c,v $
 * Revision 1.8  2005/06/22 03:02:51  arthchan2003
 * 1, Fixed doxygen documentation, 2, add  keyword.
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 09-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added glist_chkdup_*().
 * 
 * 13-Feb-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created from earlier version.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sphinxbase/glist.h"
#include "sphinxbase/ckd_alloc.h"


glist_t
glist_add_ptr(glist_t g, void *ptr)
{
    gnode_t *gn;

    gn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    gn->data.ptr = ptr;
    gn->next = g;
    return ((glist_t) gn);      /* Return the new head of the list */
}


glist_t
glist_add_int32(glist_t g, int32 val)
{
    gnode_t *gn;

    gn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    gn->data.i = (long)val;
    gn->next = g;
    return ((glist_t) gn);      /* Return the new head of the list */
}


glist_t
glist_add_uint32(glist_t g, uint32 val)
{
    gnode_t *gn;

    gn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    gn->data.ui = (unsigned long)val;
    gn->next = g;
    return ((glist_t) gn);      /* Return the new head of the list */
}


glist_t
glist_add_float32(glist_t g, float32 val)
{
    gnode_t *gn;

    gn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    gn->data.fl = (double)val;
    gn->next = g;
    return ((glist_t) gn);      /* Return the new head of the list */
}


glist_t
glist_add_float64(glist_t g, float64 val)
{
    gnode_t *gn;

    gn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    gn->data.fl = (double)val;
    gn->next = g;
    return ((glist_t) gn);      /* Return the new head of the list */
}

void
glist_free(glist_t g)
{
    gnode_t *gn;

    while (g) {
        gn = g;
        g = gn->next;
        ckd_free((void *) gn);
    }
}

int32
glist_count(glist_t g)
{
    gnode_t *gn;
    int32 n;

    for (gn = g, n = 0; gn; gn = gn->next, n++);
    return n;
}


gnode_t *
glist_tail(glist_t g)
{
    gnode_t *gn;

    if (!g)
        return NULL;

    for (gn = g; gn->next; gn = gn->next);
    return gn;
}


glist_t
glist_reverse(glist_t g)
{
    gnode_t *gn, *nextgn;
    gnode_t *rev;

    rev = NULL;
    for (gn = g; gn; gn = nextgn) {
        nextgn = gn->next;

        gn->next = rev;
        rev = gn;
    }

    return rev;
}


gnode_t *
glist_insert_ptr(gnode_t * gn, void *ptr)
{
    gnode_t *newgn;

    newgn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    newgn->data.ptr = ptr;
    newgn->next = gn->next;
    gn->next = newgn;

    return newgn;
}


gnode_t *
glist_insert_int32(gnode_t * gn, int32 val)
{
    gnode_t *newgn;

    newgn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    newgn->data.i = val;
    newgn->next = gn->next;
    gn->next = newgn;

    return newgn;
}


gnode_t *
glist_insert_uint32(gnode_t * gn, uint32 val)
{
    gnode_t *newgn;

    newgn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    newgn->data.ui = val;
    newgn->next = gn->next;

    gn->next = newgn;

    return newgn;
}


gnode_t *
glist_insert_float32(gnode_t * gn, float32 val)
{
    gnode_t *newgn;

    newgn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    newgn->data.fl = (double)val;
    newgn->next = gn->next;
    gn->next = newgn;

    return newgn;
}


gnode_t *
glist_insert_float64(gnode_t * gn, float64 val)
{
    gnode_t *newgn;

    newgn = (gnode_t *) ckd_calloc(1, sizeof(gnode_t));
    newgn->data.fl = (double)val;
    newgn->next = gn->next;
    gn->next = newgn;

    return newgn;
}

gnode_t *
gnode_free(gnode_t * gn, gnode_t * pred)
{
    gnode_t *next;

    next = gn->next;
    if (pred) {
        assert(pred->next == gn);

        pred->next = next;
    }

    ckd_free((char *) gn);

    return next;
}
