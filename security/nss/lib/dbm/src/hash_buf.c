/*-
 * Copyright (c) 1990, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. ***REMOVED*** - see
 *    ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash_buf.c  8.5 (Berkeley) 7/15/94";
#endif /* LIBC_SCCS and not lint */

/*
 * PACKAGE: hash
 *
 * DESCRIPTION:
 *  Contains buffer management
 *
 * ROUTINES:
 * External
 *  __buf_init
 *  __get_buf
 *  __buf_free
 *  __reclaim_buf
 * Internal
 *  newbuf
 */
#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh)
#include <sys/param.h>
#endif

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#include <assert.h>
#endif

#include "mcom_db.h"
#include "hash.h"
#include "page.h"
/* #include "extern.h" */

static BUFHEAD *newbuf(HTAB *, uint32, BUFHEAD *);

/* Unlink B from its place in the lru */
#define BUF_REMOVE(B)                \
    {                                \
        (B)->prev->next = (B)->next; \
        (B)->next->prev = (B)->prev; \
    }

/* Insert B after P */
#define BUF_INSERT(B, P)       \
    {                          \
        (B)->next = (P)->next; \
        (B)->prev = (P);       \
        (P)->next = (B);       \
        (B)->next->prev = (B); \
    }

#define MRU hashp->bufhead.next
#define LRU hashp->bufhead.prev

#define MRU_INSERT(B) BUF_INSERT((B), &hashp->bufhead)
#define LRU_INSERT(B) BUF_INSERT((B), LRU)

/*
 * We are looking for a buffer with address "addr".  If prev_bp is NULL, then
 * address is a bucket index.  If prev_bp is not NULL, then it points to the
 * page previous to an overflow page that we are trying to find.
 *
 * CAVEAT:  The buffer header accessed via prev_bp's ovfl field may no longer
 * be valid.  Therefore, you must always verify that its address matches the
 * address you are seeking.
 */
extern BUFHEAD *
dbm_get_buf(HTAB *hashp, uint32 addr, BUFHEAD *prev_bp, int newpage)
/* If prev_bp set, indicates a new overflow page. */
{
    register BUFHEAD *bp;
    register uint32 is_disk_mask;
    register int is_disk, segment_ndx = 0;
    SEGMENT segp = 0;

    is_disk = 0;
    is_disk_mask = 0;
    if (prev_bp) {
        bp = prev_bp->ovfl;
        if (!bp || (bp->addr != addr))
            bp = NULL;
        if (!newpage)
            is_disk = BUF_DISK;
    } else {
        /* Grab buffer out of directory */
        segment_ndx = addr & (hashp->SGSIZE - 1);

        /* valid segment ensured by dbm_call_hash() */
        segp = hashp->dir[addr >> hashp->SSHIFT];
#ifdef DEBUG
        assert(segp != NULL);
#endif

        bp = PTROF(segp[segment_ndx]);

        is_disk_mask = ISDISK(segp[segment_ndx]);
        is_disk = is_disk_mask || !hashp->new_file;
    }

    if (!bp) {
        bp = newbuf(hashp, addr, prev_bp);
        if (!bp)
            return (NULL);
        if (dbm_get_page(hashp, bp->page, addr, !prev_bp, is_disk, 0)) {
            /* free bp and its page */
            if (prev_bp) {
                /* if prev_bp is set then the new page that
                 * failed is hooked onto prev_bp as an overflow page.
                 * if we don't remove the pointer to the bad page
                 * we may try and access it later and we will die
                 * horribly because it will have already been
                 * free'd and overwritten with bogus data.
                 */
                prev_bp->ovfl = NULL;
            }
            BUF_REMOVE(bp);
            free(bp->page);
            free(bp);
            return (NULL);
        }

        if (!prev_bp) {
#if 0
            /* 16 bit windows and mac can't handle the
             * oring of the is disk flag.
             */
            segp[segment_ndx] =
                (BUFHEAD *)((ptrdiff_t)bp | is_disk_mask);
#else
            /* set the is_disk thing inside the structure
             */
            bp->is_disk = is_disk_mask;
            segp[segment_ndx] = bp;
#endif
        }
    } else {
        BUF_REMOVE(bp);
        MRU_INSERT(bp);
    }
    return (bp);
}

/*
 * We need a buffer for this page. Either allocate one, or evict a resident
 * one (if we have as many buffers as we're allowed) and put this one in.
 *
 * If newbuf finds an error (returning NULL), it also sets errno.
 */
static BUFHEAD *
newbuf(HTAB *hashp, uint32 addr, BUFHEAD *prev_bp)
{
    register BUFHEAD *bp;  /* The buffer we're going to use */
    register BUFHEAD *xbp; /* Temp pointer */
    register BUFHEAD *next_xbp;
    SEGMENT segp;
    int segment_ndx;
    uint16 oaddr, *shortp;

    oaddr = 0;
    bp = LRU;
    /*
     * If LRU buffer is pinned, the buffer pool is too small. We need to
     * allocate more buffers.
     */
    if (hashp->nbufs || (bp->flags & BUF_PIN)) {
        /* Allocate a new one */
        if ((bp = (BUFHEAD *)malloc(sizeof(BUFHEAD))) == NULL)
            return (NULL);

        /* this memset is supposedly unnecessary but lets add
         * it anyways.
         */
        memset(bp, 0xff, sizeof(BUFHEAD));

        if ((bp->page = (char *)malloc((size_t)hashp->BSIZE)) == NULL) {
            free(bp);
            return (NULL);
        }

        /* this memset is supposedly unnecessary but lets add
         * it anyways.
         */
        memset(bp->page, 0xff, (size_t)hashp->BSIZE);

        if (hashp->nbufs)
            hashp->nbufs--;
    } else {
        /* Kick someone out */
        BUF_REMOVE(bp);
        /*
         * If this is an overflow page with addr 0, it's already been
         * flushed back in an overflow chain and initialized.
         */
        if ((bp->addr != 0) || (bp->flags & BUF_BUCKET)) {
            /*
             * Set oaddr before __put_page so that you get it
             * before bytes are swapped.
             */
            shortp = (uint16 *)bp->page;
            if (shortp[0]) {
                if (shortp[0] > (hashp->BSIZE / sizeof(uint16))) {
                    return (NULL);
                }
                oaddr = shortp[shortp[0] - 1];
            }
            if ((bp->flags & BUF_MOD) && dbm_put_page(hashp, bp->page,
                                                      bp->addr, (int)IS_BUCKET(bp->flags), 0))
                return (NULL);
            /*
             * Update the pointer to this page (i.e. invalidate it).
             *
             * If this is a new file (i.e. we created it at open
             * time), make sure that we mark pages which have been
             * written to disk so we retrieve them from disk later,
             * rather than allocating new pages.
             */
            if (IS_BUCKET(bp->flags)) {
                segment_ndx = bp->addr & (hashp->SGSIZE - 1);
                segp = hashp->dir[bp->addr >> hashp->SSHIFT];
#ifdef DEBUG
                assert(segp != NULL);
#endif

                if (hashp->new_file &&
                    ((bp->flags & BUF_MOD) ||
                     ISDISK(segp[segment_ndx])))
                    segp[segment_ndx] = (BUFHEAD *)BUF_DISK;
                else
                    segp[segment_ndx] = NULL;
            }
            /*
             * Since overflow pages can only be access by means of
             * their bucket, free overflow pages associated with
             * this bucket.
             */
            for (xbp = bp; xbp->ovfl;) {
                next_xbp = xbp->ovfl;
                xbp->ovfl = 0;
                xbp = next_xbp;

                /* leave pinned pages alone, we are still using
                 * them. */
                if (xbp->flags & BUF_PIN) {
                    continue;
                }

                /* Check that ovfl pointer is up date. */
                if (IS_BUCKET(xbp->flags) ||
                    (oaddr != xbp->addr))
                    break;

                shortp = (uint16 *)xbp->page;
                if (shortp[0]) {
                    /* LJM is the number of reported
                     * pages way too much?
                     */
                    if (shortp[0] > hashp->BSIZE / sizeof(uint16))
                        return NULL;
                    /* set before __put_page */
                    oaddr = shortp[shortp[0] - 1];
                }
                if ((xbp->flags & BUF_MOD) && dbm_put_page(hashp,
                                                           xbp->page, xbp->addr, 0, 0))
                    return (NULL);
                xbp->addr = 0;
                xbp->flags = 0;
                BUF_REMOVE(xbp);
                LRU_INSERT(xbp);
            }
        }
    }

    /* Now assign this buffer */
    bp->addr = addr;
#ifdef DEBUG1
    (void)fprintf(stderr, "NEWBUF1: %d->ovfl was %d is now %d\n",
                  bp->addr, (bp->ovfl ? bp->ovfl->addr : 0), 0);
#endif
    bp->ovfl = NULL;
    if (prev_bp) {
/*
         * If prev_bp is set, this is an overflow page, hook it in to
         * the buffer overflow links.
         */
#ifdef DEBUG1
        (void)fprintf(stderr, "NEWBUF2: %d->ovfl was %d is now %d\n",
                      prev_bp->addr, (prev_bp->ovfl ? bp->ovfl->addr : 0),
                      (bp ? bp->addr : 0));
#endif
        prev_bp->ovfl = bp;
        bp->flags = 0;
    } else
        bp->flags = BUF_BUCKET;
    MRU_INSERT(bp);
    return (bp);
}

extern void
dbm_buf_init(HTAB *hashp, int32 nbytes)
{
    BUFHEAD *bfp;
    int npages;

    bfp = &(hashp->bufhead);
    npages = (nbytes + hashp->BSIZE - 1) >> hashp->BSHIFT;
    npages = PR_MAX(npages, MIN_BUFFERS);

    hashp->nbufs = npages;
    bfp->next = bfp;
    bfp->prev = bfp;
    /*
     * This space is calloc'd so these are already null.
     *
     * bfp->ovfl = NULL;
     * bfp->flags = 0;
     * bfp->page = NULL;
     * bfp->addr = 0;
     */
}

extern int
dbm_buf_free(HTAB *hashp, int do_free, int to_disk)
{
    BUFHEAD *bp;
    int status = -1;

    /* Need to make sure that buffer manager has been initialized */
    if (!LRU)
        return (0);
    for (bp = LRU; bp != &hashp->bufhead;) {
        /* Check that the buffer is valid */
        if (bp->addr || IS_BUCKET(bp->flags)) {
            if (to_disk && (bp->flags & BUF_MOD) &&
                (status = dbm_put_page(hashp, bp->page,
                                       bp->addr, IS_BUCKET(bp->flags), 0))) {

                if (do_free) {
                    if (bp->page)
                        free(bp->page);
                    BUF_REMOVE(bp);
                    free(bp);
                }

                return (status);
            }
        }
        /* Check if we are freeing stuff */
        if (do_free) {
            if (bp->page)
                free(bp->page);
            BUF_REMOVE(bp);
            free(bp);
            bp = LRU;
        } else
            bp = bp->prev;
    }
    return (0);
}

extern void
dbm_reclaim_buf(HTAB *hashp, BUFHEAD *bp)
{
    bp->ovfl = 0;
    bp->addr = 0;
    bp->flags = 0;
    BUF_REMOVE(bp);
    LRU_INSERT(bp);
}
