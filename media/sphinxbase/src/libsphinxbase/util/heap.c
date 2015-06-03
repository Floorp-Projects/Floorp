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
 * heap.c -- Generic heap structure for inserting in any and popping in sorted
 * 		order.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: heap.c,v $
 * Revision 1.4  2005/06/22 03:05:49  arthchan2003
 * 1, Fixed doxygen documentation, 2, Add  keyword.
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 05-Mar-99	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Fixed bug in heap_destroy() (in while loop exit condition).
 * 
 * 23-Dec-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Started.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sphinxbase/heap.h"
#include "sphinxbase/err.h"
#include "sphinxbase/ckd_alloc.h"

/**
 * One node on the heap
 */
typedef struct heapnode_s {
    void *data;                 /**< Application data at this node */
    int32 val;                  /**< Associated with above application data; according to which
                                   heap is sorted (in ascending order) */
    int32 nl, nr;               /**< #left/right descendants of this node (for balancing heap) */
    struct heapnode_s *l;       /**< Root of left descendant heap */
    struct heapnode_s *r;       /**< Root of right descendant heap */
} heapnode_t;

/**
 * Internal heap data structure.
 */
struct heap_s {
    heapnode_t *top;
};


#if 0
static void
heap_dump(heapnode_t * top, int32 level)
{
    int32 i;

    if (!top)
        return;

    for (i = 0; i < level; i++)
        printf("  ");
    /* print top info */
    heap_dump(top->l, level + 1);
    heap_dump(top->r, level + 1);
}
#endif


heap_t *
heap_new(void)
{
    heap_t *h = ckd_calloc(1, sizeof(*h));
    return h;
}


static heapnode_t *
subheap_insert(heapnode_t * root, void *data, int32 val)
{
    heapnode_t *h;
    void *tmpdata;
    int32 tmpval;

    if (!root) {
        h = (heapnode_t *) ckd_calloc(1, sizeof(heapnode_t));
        h->data = data;
        h->val = val;
        h->l = h->r = NULL;
        h->nl = h->nr = 0;
        return h;
    }

    /* Root already exists; if new value is less, replace root node */
    if (root->val > val) {
        tmpdata = root->data;
        tmpval = root->val;
        root->data = data;
        root->val = val;
        data = tmpdata;
        val = tmpval;
    }

    /* Insert new or old (replaced) node in right or left subtree; keep them balanced */
    if (root->nl > root->nr) {
        root->r = subheap_insert(root->r, data, val);
        root->nr++;
    }
    else {
        root->l = subheap_insert(root->l, data, val);
        root->nl++;
    }

    return root;
}


int
heap_insert(heap_t *heap, void *data, int32 val)
{
    heap->top = subheap_insert(heap->top, data, val);
    return 0;
}


static heapnode_t *
subheap_pop(heapnode_t * root)
{
    heapnode_t *l, *r;

    /* Propagate best value from below into root, if any */
    l = root->l;
    r = root->r;

    if (!l) {
        if (!r) {
            ckd_free((char *) root);
            return NULL;
        }
        else {
            root->data = r->data;
            root->val = r->val;
            root->r = subheap_pop(r);
            root->nr--;
        }
    }
    else {
        if ((!r) || (l->val < r->val)) {
            root->data = l->data;
            root->val = l->val;
            root->l = subheap_pop(l);
            root->nl--;
        }
        else {
            root->data = r->data;
            root->val = r->val;
            root->r = subheap_pop(r);
            root->nr--;
        }
    }

    return root;
}


int
heap_pop(heap_t *heap, void **data, int32 * val)
{
    if (heap->top == NULL)
        return 0;
    *data = heap->top->data;
    *val = heap->top->val;
    heap->top = subheap_pop(heap->top);
    return 1;
}


int
heap_top(heap_t *heap, void **data, int32 * val)
{
    if (heap->top == NULL)
        return 0;
    *data = heap->top->data;
    *val = heap->top->val;
    return 1;
}

static int
heap_remove_one(heap_t *heap, heapnode_t *top, void *data)
{
    if (top == NULL)
        return -1;
    else if (top->data == data) {
        assert(top == heap->top);
        heap->top = subheap_pop(heap->top);
        return 0;
    }
    if (top->l) {
        if (top->l->data == data) {
            top->l = subheap_pop(top->l);
            --top->nl;
            return 0;
        }
        else if (heap_remove_one(heap, top->l, data) == 0) {
            --top->nl;
            return 0;
        }
    }
    if (top->r) {
        if (top->r->data == data) {
            top->r = subheap_pop(top->r);
            --top->nr;
            return 0;
        }
        else if (heap_remove_one(heap, top->r, data) == 0) {
            --top->nr;
            return 0;
        }
    }
    return -1;
}

int
heap_remove(heap_t *heap, void *data)
{
    return heap_remove_one(heap, heap->top, data);
}


size_t
heap_size(heap_t *heap)
{
    if (heap->top == NULL)
        return 0;
    return heap->top->nl + heap->top->nr + 1;
}

int
heap_destroy(heap_t *heap)
{
    void *data;
    int32 val;

    /* Empty the heap and free it */
    while (heap_pop(heap, &data, &val) > 0)
        ;
    ckd_free(heap);

    return 0;
}
