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

#include <stdio.h>
#include <stdlib.h>

#include "sphinxbase/err.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/listelem_alloc.h"
#include "sphinxbase/glist.h"

/**
 * Fast linked list allocator.
 * 
 * We keep a separate linked list for each element-size.  Element-size
 * must be a multiple of pointer-size.
 *
 * Initially a block of empty elements is allocated, where the first
 * machine word in each element points to the next available element.
 * To allocate, we use this pointer to move the freelist to the next
 * element, then return the current element.
 *
 * The last element in the list starts with a NULL pointer, which is
 * used as a signal to allocate a new block of elements.
 *
 * In order to be able to actually release the memory allocated, we
 * have to add a linked list of block pointers.  This shouldn't create
 * much overhead since we never access it except when freeing the
 * allocator.
 */
struct listelem_alloc_s {
    char **freelist;            /**< ptr to first element in freelist */
    glist_t blocks;             /**< Linked list of blocks allocated. */
    glist_t blocksize;          /**< Number of elements in each block */
    size_t elemsize;            /**< Number of (char *) in element */
    size_t blk_alloc;           /**< Number of alloc operations before increasing blocksize */
    size_t n_blocks;
    size_t n_alloc;
    size_t n_freed;
};

#define MIN_ALLOC	50      /**< Minimum number of elements to allocate in one block */
#define BLKID_SHIFT     16      /**< Bit position of block number in element ID */
#define BLKID_MASK ((1<<BLKID_SHIFT)-1)

/**
 * Allocate a new block of elements.
 */
static void listelem_add_block(listelem_alloc_t *list,
			       char *caller_file, int caller_line);

listelem_alloc_t *
listelem_alloc_init(size_t elemsize)
{
    listelem_alloc_t *list;

    if ((elemsize % sizeof(void *)) != 0) {
        size_t rounded = (elemsize + sizeof(void *) - 1) & ~(sizeof(void *)-1);
        E_WARN
            ("List item size (%lu) not multiple of sizeof(void *), rounding to %lu\n",
             (unsigned long)elemsize,
             (unsigned long)rounded);
        elemsize = rounded;
    }
    list = ckd_calloc(1, sizeof(*list));
    list->freelist = NULL;
    list->blocks = NULL;
    list->elemsize = elemsize;
    /* Intent of this is to increase block size once we allocate
     * 256KiB (i.e. 1<<18). If somehow the element size is big enough
     * to overflow that, just fail, people should use malloc anyway. */
    list->blk_alloc = (1 << 18) / (MIN_ALLOC * elemsize);
    if (list->blk_alloc <= 0) {
        E_ERROR("Element size * block size exceeds 256k, use malloc instead.\n");
        ckd_free(list);
        return NULL;
    }
    list->n_alloc = 0;
    list->n_freed = 0;

    /* Allocate an initial block to minimize latency. */
    listelem_add_block(list, __FILE__, __LINE__);
    return list;
}

void
listelem_alloc_free(listelem_alloc_t *list)
{
    gnode_t *gn;
    if (list == NULL)
	return;
    for (gn = list->blocks; gn; gn = gnode_next(gn))
	ckd_free(gnode_ptr(gn));
    glist_free(list->blocks);
    glist_free(list->blocksize);
    ckd_free(list);
}

static void
listelem_add_block(listelem_alloc_t *list, char *caller_file, int caller_line)
{
    char **cpp, *cp;
    size_t j;
    int32 blocksize;

    blocksize = list->blocksize ? gnode_int32(list->blocksize) : MIN_ALLOC;
    /* Check if block size should be increased (if many requests for this size) */
    if (list->blk_alloc == 0) {
        /* See above.  No sense in allocating blocks bigger than
         * 256KiB (well, actually, there might be, but we'll worry
         * about that later). */
	blocksize <<= 1;
        if (blocksize * list->elemsize > (1 << 18))
            blocksize = (1 << 18) / list->elemsize;
	list->blk_alloc = (1 << 18) / (blocksize * list->elemsize);
    }

    /* Allocate block */
    cpp = list->freelist =
	(char **) __ckd_calloc__(blocksize, list->elemsize,
				 caller_file, caller_line);
    list->blocks = glist_add_ptr(list->blocks, cpp);
    list->blocksize = glist_add_int32(list->blocksize, blocksize);
    cp = (char *) cpp;
    /* Link up the blocks via their first machine word. */
    for (j = blocksize - 1; j > 0; --j) {
	cp += list->elemsize;
	*cpp = cp;
	cpp = (char **) cp;
    }
    /* Make sure the last element's forward pointer is NULL */
    *cpp = NULL;
    --list->blk_alloc;
    ++list->n_blocks;
}


void *
__listelem_malloc__(listelem_alloc_t *list, char *caller_file, int caller_line)
{
    char **ptr;

    /* Allocate a new block if list empty */
    if (list->freelist == NULL)
	listelem_add_block(list, caller_file, caller_line);

    /* Unlink and return first element in freelist */
    ptr = list->freelist;
    list->freelist = (char **) (*(list->freelist));
    (list->n_alloc)++;

    return (void *)ptr;
}

void *
__listelem_malloc_id__(listelem_alloc_t *list, char *caller_file,
                       int caller_line, int32 *out_id)
{
    char **ptr;

    /* Allocate a new block if list empty */
    if (list->freelist == NULL)
	listelem_add_block(list, caller_file, caller_line);

    /* Unlink and return first element in freelist */
    ptr = list->freelist;
    list->freelist = (char **) (*(list->freelist));
    (list->n_alloc)++;

    if (out_id) {
        int32 blksize, blkidx, ptridx;
        gnode_t *gn, *gn2;
        char **block;

        gn2 = list->blocksize;
        block = NULL;
        blkidx = 0;
        for (gn = list->blocks; gn; gn = gnode_next(gn)) {
            block = gnode_ptr(gn);
            blksize = gnode_int32(gn2) * list->elemsize / sizeof(*block);
            if (ptr >= block && ptr < block + blksize)
                break;
            gn2 = gnode_next(gn2);
            ++blkidx;
        }
        if (gn == NULL) {
            E_ERROR("Failed to find block index for pointer %p!\n", ptr);
        }
        ptridx = (ptr - block) / (list->elemsize / sizeof(*block));
        E_DEBUG(4,("ptr %p block %p blkidx %d ptridx %d\n",
                   ptr, block, list->n_blocks - blkidx - 1, ptridx));
        *out_id = ((list->n_blocks - blkidx - 1) << BLKID_SHIFT) | ptridx;
    }

    return ptr;
}

void *
listelem_get_item(listelem_alloc_t *list, int32 id)
{
    int32 blkidx, ptridx, i;
    gnode_t *gn;

    blkidx = (id >> BLKID_SHIFT) & BLKID_MASK;
    ptridx = id & BLKID_MASK;

    i = 0;
    blkidx = list->n_blocks - blkidx;
    for (gn = list->blocks; gn; gn = gnode_next(gn)) {
        if (++i == blkidx)
            break;
    }
    if (gn == NULL) {
        E_ERROR("Failed to find block index %d\n", blkidx);
        return NULL;
    }

    return (void *)((char **)gnode_ptr(gn)
                    + ptridx * (list->elemsize / sizeof(void *)));
}

void
__listelem_free__(listelem_alloc_t *list, void *elem,
                  char *caller_file, int caller_line)
{
    char **cpp;

    /*
     * Insert freed item at head of list.
     */
    cpp = (char **) elem;
    *cpp = (char *) list->freelist;
    list->freelist = cpp;
    (list->n_freed)++;
}


void
listelem_stats(listelem_alloc_t *list)
{
    gnode_t *gn, *gn2;
    char **cpp;
    size_t n;

    E_INFO("Linklist stats:\n");
    for (n = 0, cpp = list->freelist; cpp;
         cpp = (char **) (*cpp), n++);
    E_INFO
        ("elemsize %lu, #alloc %lu, #freed %lu, #freelist %lu\n",
         (unsigned long)list->elemsize,
         (unsigned long)list->n_alloc,
         (unsigned long)list->n_freed,
         (unsigned long)n);
    E_INFO("Allocated blocks:\n");
    gn2 = list->blocksize;
    for (gn = list->blocks; gn; gn = gnode_next(gn)) {
	E_INFO("%p (%d * %d bytes)\n", gnode_ptr(gn), gnode_int32(gn2), list->elemsize);
        gn2 = gnode_next(gn2);
    }
}
