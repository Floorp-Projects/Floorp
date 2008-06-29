/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#ifdef WIN32
#include <windef.h>
#include <winbase.h>
#endif

#include "prclist.h"
#include "prbit.h"

#include "prtypes.h"
#include "prenv.h"
#include "prgc.h"
#include "prthread.h"
#include "prlog.h"
#include "prlong.h"
#include "prinrval.h"
#include "prprf.h"
#include "gcint.h"

#if defined(XP_MAC)
#include "pprthred.h"
#else
#include "private/pprthred.h"
#endif

typedef void (*PRFileDumper)(FILE *out, PRBool detailed);

PR_EXTERN(void)
PR_DumpToFile(char* filename, char* msg, PRFileDumper dump, PRBool detailed);

/*
** Mark&sweep garbage collector. Supports objects that require
** finalization, objects that can have a single weak link, and special
** objects that require care during sweeping.
*/

PRLogModuleInfo *_pr_msgc_lm;
PRLogModuleInfo* GC;

static PRInt32 _pr_pageShift;
static PRInt32 _pr_pageSize;

#ifdef DEBUG
#define GCMETER
#endif
#ifdef DEBUG_jwz
# undef GCMETER
#endif /* 1 */

#ifdef GCMETER
#define METER(x) x
#else
#define METER(x)
#endif

/*
** Make this constant bigger to reduce the amount of recursion during
** garbage collection.
*/
#define MAX_SCAN_Q    100L

#if defined(XP_PC) && !defined(WIN32)
#define MAX_SEGS            400L
#define MAX_SEGMENT_SIZE    (65536L - 4096L)
#define SEGMENT_SIZE        (65536L - 4096L)
#define MAX_ALLOC_SIZE      (65536L - 4096L)
#else
#define MAX_SEGS                    400L
#define MAX_SEGMENT_SIZE            (2L * 256L * 1024L)
#define SEGMENT_SIZE                (1L * 256L * 1024L)
#define MAX_ALLOC_SIZE              (4L * 1024L * 1024L)
#endif  

/* 
 * The highest value that can fit into a signed integer. This
 * is used to prevent overflow of allocation size in alloc routines.
 */
 
#define MAX_INT ((1UL << (PR_BITS_PER_INT - 1)) - 1)

/* 
 * On 32-bit machines, only 22 bits are used in the cibx integer to
 * store size since 8 bits of the integer are used to store type, and
 * of the remainder, 2 are user defined. Max allocation size = 2^22 -1
 */

#define MAX_ALLOC ( (1L << (PR_BYTES_PER_WORD_LOG2 + WORDS_BITS )) -1)

/* The minimum percentage of free heap space after a collection. If
   the amount of free space doesn't meet this criteria then we will
   attempt to grow the heap */
#ifdef XP_MAC
#define MIN_FREE_THRESHOLD_AFTER_GC 10L
#else
#define MIN_FREE_THRESHOLD_AFTER_GC 20L
#endif

static PRInt32 segmentSize = SEGMENT_SIZE;

static PRInt32 collectorCleanupNeeded;

#ifdef GCMETER
PRUint32 _pr_gcMeter;

#define _GC_METER_STATS         0x01L
#define _GC_METER_GROWTH        0x02L
#define _GC_METER_FREE_LIST     0x04L
#endif

/************************************************************************/

#define LINEAR_BIN_EXPONENT 5
#define NUM_LINEAR_BINS ((PRUint32)1 << LINEAR_BIN_EXPONENT)
#define FIRST_LOG_BIN (NUM_LINEAR_BINS - LINEAR_BIN_EXPONENT)

/* Each free list bin holds a chunk of memory sized from
   2^n to (2^(n+1))-1 inclusive. */
#define NUM_BINS        (FIRST_LOG_BIN + 32)

/*
 * Find the bin number for a given size (in bytes). This does not round up as
 * values from 2^n to (2^(n+1))-1 share the same bin.
 */
#define InlineBinNumber(_bin,_bytes)  \
{  \
    PRUint32 _t, _n = (PRUint32) _bytes / 4;  \
    if (_n < NUM_LINEAR_BINS) {  \
        _bin = _n;  \
    } else {  \
        _bin = FIRST_LOG_BIN;  \
        if ((_t = (_n >> 16)) != 0) { _bin += 16; _n = _t; }  \
        if ((_t = (_n >> 8)) != 0)  { _bin += 8; _n = _t; }  \
        if ((_t = (_n >> 4)) != 0)  { _bin += 4; _n = _t; }  \
        if ((_t = (_n >> 2)) != 0) { _bin += 2; _n = _t; }  \
        if ((_n >> 1) != 0) _bin++;  \
    }  \
}

#define BIG_ALLOC       16384L

#define MIN_FREE_CHUNK_BYTES    ((PRInt32)sizeof(GCFreeChunk))

/* Note: fix code in PR_AllocMemory if you change the size of GCFreeChunk
   so that it zeros the right number of words */
typedef struct GCFreeChunk {
    struct GCFreeChunk *next;
    struct GCSeg *segment;
    PRInt32 chunkSize;
} GCFreeChunk;

typedef struct GCSegInfo {
    struct GCSegInfo *next;
    char *base;
    char *limit;
    PRWord *hbits;
    int fromMalloc;
} GCSegInfo;
    
typedef struct GCSeg {
    char *base;
    char *limit;
    PRWord *hbits;
    GCSegInfo *info;
} GCSeg;

#ifdef GCMETER
typedef struct GCMeter {
    PRInt32 allocBytes;
    PRInt32 wastedBytes;
    PRInt32 numFreeChunks;
    PRInt32 skippedFreeChunks;
} GCMeter;
static GCMeter meter;
#endif

/*
** There is one of these for each segment of GC'able memory.
*/
static GCSeg segs[MAX_SEGS];
static GCSegInfo *freeSegs;
static GCSeg* lastInHeap;
static int nsegs;

static GCFreeChunk *bins[NUM_BINS];
static PRInt32 minBin;
static PRInt32 maxBin;

/*
** Scan Q used to avoid deep recursion when scanning live objects for
** heap pointers
*/
typedef struct GCScanQStr {
    PRWord *q[MAX_SCAN_Q];
    int queued;
} GCScanQ;

static GCScanQ *pScanQ;

#ifdef GCMETER
PRInt32 _pr_maxScanDepth;
PRInt32 _pr_scanDepth;
#endif

/*
** Keeps track of the number of bytes allocated via the BigAlloc() 
** allocator.  When the number of bytes allocated, exceeds the 
** BIG_ALLOC_GC_SIZE, then a GC will occur before the next allocation
** is done...
*/
#define BIG_ALLOC_GC_SIZE       (4*SEGMENT_SIZE)
static PRWord bigAllocBytes = 0;

/*
** There is one GC header word in front of each GC allocated object.  We
** use it to contain information about the object (what TYPEIX to use for
** scanning it, how big it is, it's mark status, and if it's a root).
*/
#define TYPEIX_BITS    8L
#define WORDS_BITS    20L
#define MAX_CBS        (1L << GC_TYPEIX_BITS)
#define MAX_WORDS    (1L << GC_WORDS_BITS)
#define TYPEIX_SHIFT    24L
#define MAX_TYPEIX    ((1L << TYPEIX_BITS) - 1L)
#define TYPEIX_MASK    PR_BITMASK(TYPEIX_BITS)
#define WORDS_SHIFT    2L
#define WORDS_MASK    PR_BITMASK(WORDS_BITS)
#define MARK_BIT    1L
#define FINAL_BIT    2L

/* Two bits per object header are reserved for the user of the memory
   system to store information into. */
#define GC_USER_BITS_SHIFT 22L
#define GC_USER_BITS    0x00c00000L

#define MAKE_HEADER(_cbix,_words)              \
    ((PRWord) (((unsigned long)(_cbix) << TYPEIX_SHIFT) \
         | ((unsigned long)(_words) << WORDS_SHIFT)))

#define GET_TYPEIX(_h) \
    (((PRUword)(_h) >> TYPEIX_SHIFT) & 0xff)

#define MARK(_sp,_p) \
    (((PRWord *)(_p))[0] |= MARK_BIT)
#define IS_MARKED(_sp,_p) \
    (((PRWord *)(_p))[0] & MARK_BIT)
#define OBJ_BYTES(_h) \
    (((PRInt32) (_h) & 0x003ffffcL) << (PR_BYTES_PER_WORD_LOG2-2L))

#define GC_GET_USER_BITS(_h) (((_h) & GC_USER_BITS) >> GC_USER_BITS_SHIFT)

/************************************************************************/

/*
** Mark the start of an object in a segment. Note that we mark the header
** word (which we always have), not the data word (which we may not have
** for empty objects).
** XXX tune: put subtract of _sp->base into _sp->hbits pointer?
*/
#if !defined(WIN16)
#define SET_HBIT(_sp,_ph) \
    SET_BIT((_sp)->hbits, (((PRWord*)(_ph)) - ((PRWord*) (_sp)->base)))

#define CLEAR_HBIT(_sp,_ph) \
    CLEAR_BIT((_sp)->hbits, (((PRWord*)(_ph)) - ((PRWord*) (_sp)->base)))

#define IS_HBIT(_sp,_ph) \
    TEST_BIT((_sp)->hbits, (((PRWord*)(_ph)) - ((PRWord*) (_sp)->base)))
#else

#define SET_HBIT(_sp,_ph) set_hbit(_sp,_ph)

#define CLEAR_HBIT(_sp,_ph) clear_hbit(_sp,_ph)

#define IS_HBIT(_sp,_ph) is_hbit(_sp,_ph)

static void
set_hbit(GCSeg *sp, PRWord *p)
{
    unsigned int distance;
    unsigned int index;
    PRWord     mask;

        PR_ASSERT( SELECTOROF(p) == SELECTOROF(sp->base) );
        PR_ASSERT( OFFSETOF(p)   >= OFFSETOF(sp->base) );

        distance = (OFFSETOF(p) - OFFSETOF(sp->base)) >> 2;
    index    = distance >> PR_BITS_PER_WORD_LOG2;
    mask     = 1L << (distance&(PR_BITS_PER_WORD-1));

    sp->hbits[index] |= mask;
}

static void
clear_hbit(GCSeg *sp, PRWord *p)
{
    unsigned int distance;
    unsigned int index;
    PRWord    mask;

        PR_ASSERT( SELECTOROF(p) == SELECTOROF(sp->base) );
        PR_ASSERT( OFFSETOF(p)   >= OFFSETOF(sp->base) );

        distance = (OFFSETOF(p) - OFFSETOF(sp->base)) >> 2;
    index    = distance >> PR_BITS_PER_WORD_LOG2;
    mask     = 1L << (distance&(PR_BITS_PER_WORD-1));

    sp->hbits[index] &= ~mask;
}

static int
is_hbit(GCSeg *sp, PRWord *p)
{
    unsigned int distance;
    unsigned int index;
    PRWord    mask;

        PR_ASSERT( SELECTOROF(p) == SELECTOROF(sp->base) );
        PR_ASSERT( OFFSETOF(p)   >= OFFSETOF(sp->base) );

        distance = (OFFSETOF(p) - OFFSETOF(sp->base)) >> 2;
    index    = distance >> PR_BITS_PER_WORD_LOG2;
    mask     = 1L << (distance&(PR_BITS_PER_WORD-1));

    return ((sp->hbits[index] & mask) != 0);
}


#endif  /* WIN16 */

/*
** Given a pointer into this segment, back it up until we are at the
** start of the object the pointer points into. Each heap segment has a
** bitmap that has one bit for each word of the objects it contains.  The
** bit's are set for the firstword of an object, and clear for it's other
** words.
*/
static PRWord *FindObject(GCSeg *sp, PRWord *p)
{
    PRWord *base;
    
    /* Align p to it's proper boundary before we start fiddling with it */
    p = (PRWord*) ((PRWord)p & ~(PR_BYTES_PER_WORD-1L));

    base = (PRWord *) sp->base;
#if defined(WIN16)
    PR_ASSERT( SELECTOROF(p) == SELECTOROF(base));
#endif
    do {
    if (IS_HBIT(sp, p)) {
        return (p);
    }
    p--;
    } while ( p >= base );

    /* Heap is corrupted! */
    _GCTRACE(GC_TRACE, ("ERROR: The heap is corrupted!!! aborting now!"));
    abort();
    return NULL;
}

/************************************************************************/
#if !defined(XP_PC) || defined(XP_OS2)
#define OutputDebugString(msg)
#endif 

#if !defined(WIN16)
#define IN_SEGMENT(_sp, _p)             \
    ((((char *)(_p)) >= (_sp)->base) &&    \
     (((char *)(_p)) < (_sp)->limit))
#else
#define IN_SEGMENT(_sp, _p)                  \
    ((((PRWord)(_p)) >= ((PRWord)(_sp)->base)) && \
     (((PRWord)(_p)) < ((PRWord)(_sp)->limit)))
#endif

static GCSeg *InHeap(void *p)
{
    GCSeg *sp, *esp;

    if (lastInHeap && IN_SEGMENT(lastInHeap, p)) {
    return lastInHeap;
    }

    sp = segs;
    esp = segs + nsegs;
    for (; sp < esp; sp++) {
    if (IN_SEGMENT(sp, p)) {
        lastInHeap = sp;
        return sp;
    }
    }
    return 0;
}

/*
** Grow the heap by allocating another segment. Fudge the requestedSize
** value to try to pre-account for the HBITS.
*/
static GCSeg* DoGrowHeap(PRInt32 requestedSize, PRBool exactly)
{
    GCSeg *sp;
    GCSegInfo *segInfo;
    GCFreeChunk *cp;
    char *base;
    PRWord *hbits;
    PRInt32 nhbytes, nhbits;
    PRUint32 allocSize;

    if (nsegs == MAX_SEGS) {
    /* No room for more segments */
    return 0;
    }

    segInfo = (GCSegInfo*) PR_MALLOC(sizeof(GCSegInfo));
#ifdef DEBUG
    {
    char str[256];
    sprintf(str, "[1] Allocated %ld bytes at %p\n",
        (long) sizeof(GCSegInfo), segInfo);
    OutputDebugString(str);
    }
#endif
    if (!segInfo) {
    return 0;
    }

#if defined(WIN16)
    if (requestedSize > segmentSize) {
    PR_DELETE(segInfo);
    return 0;
    }
#endif

    /* Get more memory from the OS */
    if (exactly) {
    allocSize = requestedSize;
    base = (char *) PR_MALLOC(requestedSize);
    } else {
    allocSize = requestedSize;
    allocSize = (allocSize + _pr_pageSize - 1L) >> _pr_pageShift;
    allocSize <<= _pr_pageShift;
    base = (char*)_MD_GrowGCHeap(&allocSize);
    }
    if (!base) {
    PR_DELETE(segInfo);
    return 0;
    }

    nhbits = (PRInt32)(
        (allocSize + PR_BYTES_PER_WORD - 1L) >> PR_BYTES_PER_WORD_LOG2);
    nhbytes = ((nhbits + PR_BITS_PER_WORD - 1L) >> PR_BITS_PER_WORD_LOG2)
    * sizeof(PRWord);

    /* Get bitmap memory from malloc heap */
#if defined(WIN16)
    PR_ASSERT( nhbytes < MAX_ALLOC_SIZE );
#endif
    hbits = (PRWord *) PR_CALLOC((PRUint32)nhbytes);
    if (!hbits) {
    /* Loser! */
    PR_DELETE(segInfo);
    if (exactly) {
        PR_DELETE(base);
    } else {
      /* XXX do something about this */
      /* _MD_FreeGCSegment(base, allocSize); */
    }
    return 0;
    }

    /*
    ** Setup new segment.
    */
    sp = &segs[nsegs++];
    segInfo->base = sp->base = base;
    segInfo->limit = sp->limit = base + allocSize;
    segInfo->hbits = sp->hbits = hbits;
    sp->info = segInfo;
    segInfo->fromMalloc = exactly;
    memset(base, 0, allocSize);

#ifdef GCMETER
    if (_pr_gcMeter & _GC_METER_GROWTH) {
        fprintf(stderr, "[GC: new segment base=%p size=%ld]\n",
                sp->base, (long) allocSize);
    }
#endif    

    _pr_gcData.allocMemory += allocSize;
    _pr_gcData.freeMemory  += allocSize;

    if (!exactly) {
    PRInt32 bin;

        /* Put free memory into a freelist bin */
        cp = (GCFreeChunk *) base;
        cp->segment = sp;
        cp->chunkSize = allocSize;
        InlineBinNumber(bin, allocSize)
        cp->next = bins[bin];
        bins[bin] = cp;
    if (bin < minBin) minBin = bin;
    if (bin > maxBin) maxBin = bin;
    } else {
        /*
        ** When exactly allocating the entire segment is given over to a
        ** single object to prevent fragmentation
        */
    }

    if (!_pr_gcData.lowSeg) {
    _pr_gcData.lowSeg  = (PRWord*) sp->base;
    _pr_gcData.highSeg = (PRWord*) sp->limit;
    } else {
    if ((PRWord*)sp->base < _pr_gcData.lowSeg) {
        _pr_gcData.lowSeg = (PRWord*) sp->base;
    }
    if ((PRWord*)sp->limit > _pr_gcData.highSeg) {
        _pr_gcData.highSeg = (PRWord*) sp->limit;
    }
    }

    /* 
    ** Get rid of the GC pointer in case it shows up in some uninitialized
    ** local stack variable later (while scanning the C stack looking for
    ** roots).
    */ 
    memset(&base, 0, sizeof(base));  /* optimizers beware */

    PR_LOG(_pr_msgc_lm, PR_LOG_WARNING, ("grow heap: total gc memory now %d",
                      _pr_gcData.allocMemory));

    return sp;
}

#ifdef USE_EXTEND_HEAP
static PRBool ExtendHeap(PRInt32 requestedSize) {
  GCSeg* sp;
  PRUint32 allocSize;
  PRInt32 oldSize, newSize;
  PRInt32 newHBits, newHBytes;
  PRInt32 oldHBits, oldHBytes;
  PRWord* hbits;
  GCFreeChunk* cp;
  PRInt32 bin;

  /* Can't extend nothing */
  if (nsegs == 0) return PR_FALSE;

  /* Round up requested size to the size of a page */
  allocSize = (PRUint32) requestedSize;
  allocSize = (allocSize + _pr_pageSize - 1L) >> _pr_pageShift;
  allocSize <<= _pr_pageShift;

  /* Malloc some memory for the new hbits array */
  sp = segs;
  oldSize = sp->limit - sp->base;
  newSize = oldSize + allocSize;
  newHBits = (newSize + PR_BYTES_PER_WORD - 1L) >> PR_BYTES_PER_WORD_LOG2;
  newHBytes = ((newHBits + PR_BITS_PER_WORD - 1L) >> PR_BITS_PER_WORD_LOG2)
    * sizeof(PRWord);
  hbits = (PRWord*) PR_MALLOC(newHBytes);
  if (0 == hbits) return PR_FALSE;

  /* Attempt to extend the last segment by the desired amount */
  if (_MD_ExtendGCHeap(sp->base, oldSize, newSize)) {
    oldHBits = (oldSize + PR_BYTES_PER_WORD - 1L) >> PR_BYTES_PER_WORD_LOG2;
    oldHBytes = ((oldHBits + PR_BITS_PER_WORD - 1L) >> PR_BITS_PER_WORD_LOG2)
      * sizeof(PRWord);

    /* Copy hbits from old memory into new memory */
    memset(hbits, 0, newHBytes);
    memcpy(hbits, sp->hbits, oldHBytes);
    PR_DELETE(sp->hbits);
    memset(sp->base + oldSize, 0, allocSize);

    /* Adjust segment state */
    sp->limit += allocSize;
    sp->hbits = hbits;
    sp->info->limit = sp->limit;
    sp->info->hbits = hbits;

    /* Put free memory into a freelist bin */
    cp = (GCFreeChunk *) (sp->base + oldSize);
    cp->segment = sp;
    cp->chunkSize = allocSize;
    InlineBinNumber(bin, allocSize)
    cp->next = bins[bin];
    bins[bin] = cp;
    if (bin < minBin) minBin = bin;
    if (bin > maxBin) maxBin = bin;

    /* Prevent a pointer that points to the free memory from showing
       up on the call stack later on */
    memset(&cp, 0, sizeof(cp));

    /* Update heap brackets and counters */
    if ((PRWord*)sp->limit > _pr_gcData.highSeg) {
      _pr_gcData.highSeg = (PRWord*) sp->limit;
    }
    _pr_gcData.allocMemory += allocSize;
    _pr_gcData.freeMemory  += allocSize;

    return PR_TRUE;
  }
  PR_DELETE(hbits);
  return PR_FALSE;
}
#endif /* USE_EXTEND_HEAP */

static GCSeg *GrowHeapExactly(PRInt32 requestedSize)
{
    GCSeg *sp = DoGrowHeap(requestedSize, PR_TRUE);
    return sp;
}

static PRBool GrowHeap(PRInt32 requestedSize)
{
  void *p;
#ifdef USE_EXTEND_HEAP
  if (ExtendHeap(requestedSize)) {
    return PR_TRUE;
  }
#endif
  p = DoGrowHeap(requestedSize, PR_FALSE);
  return (p != NULL ? PR_TRUE : PR_FALSE);
}

/*
** Release a segment when it is entirely free.
*/
static void ShrinkGCHeap(GCSeg *sp)
{
#ifdef GCMETER
    if (_pr_gcMeter & _GC_METER_GROWTH) {
        fprintf(stderr, "[GC: free segment base=%p size=%ld]\n",
                sp->base, (long) (sp->limit - sp->base));
    }
#endif    

    /*
     * Put segment onto free seginfo list (we can't call free right now
     * because we have the GC lock and all of the other threads are
     * suspended; if one of them has the malloc lock we would deadlock)
     */
    sp->info->next = freeSegs;
    freeSegs = sp->info;
    collectorCleanupNeeded = 1;
    _pr_gcData.allocMemory -= sp->limit - sp->base;
    if (sp == lastInHeap) lastInHeap = 0;

    /* Squish out disappearing segment from segment table */
    --nsegs;
    if ((sp - segs) != nsegs) {
        *sp = segs[nsegs];
    } else {
        sp->base = 0;
        sp->limit = 0;
        sp->hbits = 0;
    sp->info = 0;
    }

    /* Recalculate the lowSeg and highSeg values */
    _pr_gcData.lowSeg  = (PRWord*) segs[0].base;
    _pr_gcData.highSeg = (PRWord*) segs[0].limit;
    for (sp = segs; sp < &segs[nsegs]; sp++) {
    if ((PRWord*)sp->base < _pr_gcData.lowSeg) {
        _pr_gcData.lowSeg = (PRWord*) sp->base;
    }
    if ((PRWord*)sp->limit > _pr_gcData.highSeg) {
        _pr_gcData.highSeg = (PRWord*) sp->limit;
    }
    }
}

static void FreeSegments(void)
{
    GCSegInfo *si;

    while (0 != freeSegs) {
    LOCK_GC();
    si = freeSegs;
    if (si) {
        freeSegs = si->next;
    }
    UNLOCK_GC();

    if (!si) {
        break;
    }
    PR_DELETE(si->base);
    PR_DELETE(si->hbits);
    PR_DELETE(si);
    }
}

/************************************************************************/

void ScanScanQ(GCScanQ *iscan)
{
    PRWord *p;
    PRWord **pp;
    PRWord **epp;
    GCScanQ nextQ, *scan, *next, *temp;
    CollectorType *ct;

    if (!iscan->queued) return;

    _GCTRACE(GC_MARK, ("begin scanQ @ 0x%x (%d)", iscan, iscan->queued));
    scan = iscan;
    next = &nextQ;
    while (scan->queued) {
	_GCTRACE(GC_MARK, ("continue scanQ @ 0x%x (%d)", scan, scan->queued));
    /*
     * Set pointer to current scanQ so that _pr_gcData.livePointer
     * can find it.
     */
    pScanQ = next;
    next->queued = 0;

    /* Now scan the scan Q */
    pp = scan->q;
    epp = &scan->q[scan->queued];
    scan->queued = 0;
    while (pp < epp) {
        p = *pp++;
        ct = &_pr_collectorTypes[GET_TYPEIX(p[0])];
        PR_ASSERT(0 != ct->gctype.scan);
        /* Scan object ... */
        (*ct->gctype.scan)(p + 1);
    }

    /* Exchange pointers so that we scan next */
    temp = scan;
    scan = next;
    next = temp;
    }

    pScanQ = iscan;
    PR_ASSERT(nextQ.queued == 0);
    PR_ASSERT(iscan->queued == 0);
}

/*
** Called during root finding step to identify "root" pointers into the
** GC heap. First validate if it is a real heap pointer and then mark the
** object being pointed to and add it to the scan Q for eventual
** scanning.
*/
static void PR_CALLBACK ProcessRootBlock(void **base, PRInt32 count)
{
    GCSeg *sp;
    PRWord *p0, *p, h, tix, *low, *high, *segBase;
    CollectorType *ct;
#ifdef DEBUG
    void **base0 = base;
#endif

    low = _pr_gcData.lowSeg;
    high = _pr_gcData.highSeg;
    while (--count >= 0) {
        p0 = (PRWord*) *base++;
        /*
        ** XXX:  
        ** Until Win16 maintains lowSeg and highSeg correctly,
        ** (ie. lowSeg=MIN(all segs) and highSeg = MAX(all segs))
        ** Allways scan through the segment list
        */
#if !defined(WIN16)
        if (p0 < low) continue;                  /* below gc heap */
        if (p0 >= high) continue;                /* above gc heap */
#endif
        /* NOTE: inline expansion of InHeap */
        /* Find segment */
    sp = lastInHeap;
        if (!sp || !IN_SEGMENT(sp,p0)) {
            GCSeg *esp;
            sp = segs;
        esp = segs + nsegs;
            for (; sp < esp; sp++) {
                if (IN_SEGMENT(sp, p0)) {
                    lastInHeap = sp;
                    goto find_object;
                }
            }
            continue;
        }

      find_object:
        /* NOTE: Inline expansion of FindObject */
        /* Align p to it's proper boundary before we start fiddling with it */
        p = (PRWord*) ((PRWord)p0 & ~(PR_BYTES_PER_WORD-1L));
        segBase = (PRWord *) sp->base;
        do {
            if (IS_HBIT(sp, p)) {
                goto winner;
            }
            p--;
        } while (p >= segBase);

        /*
        ** We have a pointer into the heap, but it has no header
        ** bit. This means that somehow the very first object in the heap
        ** doesn't have a header. This is impossible so when debugging
        ** lets abort.
        */
#ifdef DEBUG
        PR_Abort();
#endif

      winner:
        h = p[0];
        if ((h & MARK_BIT) == 0) {
#ifdef DEBUG
            _GCTRACE(GC_ROOTS,
            ("root 0x%p (%d) base0=%p off=%d",
             p, OBJ_BYTES(h), base0, (base-1) - base0));
#endif

            /* Mark the root we just found */
            p[0] = h | MARK_BIT;

            /*
         * See if object we just found needs scanning. It must
         * have a scan function to be placed on the scanQ.
         */
            tix = (PRWord)GET_TYPEIX(h);
        ct = &_pr_collectorTypes[tix];
        if (0 == ct->gctype.scan) {
        continue;
        }

            /*
            ** Put a pointer onto the scan Q. We use the scan Q to avoid
            ** deep recursion on the C call stack. Objects are added to
            ** the scan Q until the scan Q fills up. At that point we
            ** make a call to ScanScanQ which proceeds to scan each of
            ** the objects in the Q. This limits the recursion level by a
            ** large amount though the stack frames get larger to hold
            ** the GCScanQ's.
            */
            pScanQ->q[pScanQ->queued++] = p;
            if (pScanQ->queued == MAX_SCAN_Q) {
                METER(_pr_scanDepth++);
                ScanScanQ(pScanQ);
            }
        }
    }
}

static void PR_CALLBACK ProcessRootPointer(void *ptr)
{
  PRWord *p0, *p, h, tix, *segBase;
  GCSeg* sp;
  CollectorType *ct;

  p0 = (PRWord*) ptr;

  /*
  ** XXX:  
  ** Until Win16 maintains lowSeg and highSeg correctly,
  ** (ie. lowSeg=MIN(all segs) and highSeg = MAX(all segs))
  ** Allways scan through the segment list
  */
#if !defined(WIN16)
  if (p0 < _pr_gcData.lowSeg) return;                  /* below gc heap */
  if (p0 >= _pr_gcData.highSeg) return;                /* above gc heap */
#endif

  /* NOTE: inline expansion of InHeap */
  /* Find segment */
  sp = lastInHeap;
  if (!sp || !IN_SEGMENT(sp,p0)) {
    GCSeg *esp;
    sp = segs;
    esp = segs + nsegs;
    for (; sp < esp; sp++) {
      if (IN_SEGMENT(sp, p0)) {
    lastInHeap = sp;
    goto find_object;
      }
    }
    return;
  }

 find_object:
  /* NOTE: Inline expansion of FindObject */
  /* Align p to it's proper boundary before we start fiddling with it */
    p = (PRWord*) ((PRWord)p0 & ~(BYTES_PER_WORD-1L));
    segBase = (PRWord *) sp->base;
    do {
      if (IS_HBIT(sp, p)) {
    goto winner;
      }
      p--;
    } while (p >= segBase);

    /*
    ** We have a pointer into the heap, but it has no header
    ** bit. This means that somehow the very first object in the heap
    ** doesn't have a header. This is impossible so when debugging
    ** lets abort.
    */
#ifdef DEBUG
    PR_Abort();
#endif

 winner:
  h = p[0];
  if ((h & MARK_BIT) == 0) {
#ifdef DEBUG
    _GCTRACE(GC_ROOTS, ("root 0x%p (%d)", p, OBJ_BYTES(h)));
#endif

    /* Mark the root we just found */
    p[0] = h | MARK_BIT;

    /*
     * See if object we just found needs scanning. It must
     * have a scan function to be placed on the scanQ.
     */
    tix = (PRWord)GET_TYPEIX(h);
    ct = &_pr_collectorTypes[tix];
    if (0 == ct->gctype.scan) {
      return;
    }

    /*
    ** Put a pointer onto the scan Q. We use the scan Q to avoid
    ** deep recursion on the C call stack. Objects are added to
    ** the scan Q until the scan Q fills up. At that point we
    ** make a call to ScanScanQ which proceeds to scan each of
    ** the objects in the Q. This limits the recursion level by a
    ** large amount though the stack frames get larger to hold
    ** the GCScanQ's.
    */
    pScanQ->q[pScanQ->queued++] = p;
    if (pScanQ->queued == MAX_SCAN_Q) {
      METER(_pr_scanDepth++);
      ScanScanQ(pScanQ);
    }
  }
}

/************************************************************************/

/*
** Empty the freelist for each segment. This is done to make sure that
** the root finding step works properly (otherwise, if we had a pointer
** into a free section, we might not find its header word and abort in
** FindObject)
*/
static void EmptyFreelists(void)
{
    GCFreeChunk *cp;
    GCFreeChunk *next;
    GCSeg *sp;
    PRWord *p;
    PRInt32 chunkSize;
    PRInt32 bin;

    /*
    ** Run over the freelist and make all of the free chunks look like
    ** object debris.
    */
    for (bin = 0; bin <= NUM_BINS-1; bin++) {
        cp = bins[bin];
        while (cp) {
            next = cp->next;
            sp = cp->segment;
            chunkSize = cp->chunkSize >> BYTES_PER_WORD_LOG2;
            p = (PRWord*) cp;
            PR_ASSERT(chunkSize != 0);
            p[0] = MAKE_HEADER(FREE_MEMORY_TYPEIX, chunkSize);
            SET_HBIT(sp, p);
            cp = next;
        }
        bins[bin] = 0;
    }
    minBin = NUM_BINS - 1;
    maxBin = 0;
}

typedef struct GCBlockEnd {
    PRInt32	check;
#ifdef GC_CHECK
    PRInt32	requestedBytes;
#endif
#ifdef GC_STATS
    PRInt32	bin;
    PRInt64	allocTime; 
#endif
#ifdef GC_TRACEROOTS
    PRInt32	traceGeneration;	
#endif
} GCBlockEnd;

#define PR_BLOCK_END	0xDEADBEEF

/************************************************************************/

#ifdef GC_STATS

typedef struct GCStat {
    PRInt32	nallocs;
    double	allocTime;
    double	allocTimeVariance;
    PRInt32	nfrees;
    double	lifetime;
    double	lifetimeVariance;
} GCStat;

#define GCSTAT_BINS	NUM_BINS

GCStat gcstats[GCSTAT_BINS];

#define GCLTFREQ_BINS	NUM_BINS

PRInt32 gcltfreq[GCSTAT_BINS][GCLTFREQ_BINS];

#include <math.h>

static char* 
pr_GetSizeString(PRUint32 size)
{
    char* sizeStr;
    if (size < 1024)
	sizeStr = PR_smprintf("<= %ld", size);
    else if (size < 1024 * 1024)
	sizeStr = PR_smprintf("<= %ldk", size / 1024);
    else 
	sizeStr = PR_smprintf("<= %ldM", size / (1024 * 1024));
    return sizeStr;
}

static void
pr_FreeSizeString(char *sizestr)
{
	PR_smprintf_free(sizestr);
}


static void
pr_PrintGCAllocStats(FILE* out)
{
    PRInt32 i, j;
    _PR_DebugPrint(out, "\n--Allocation-Stats-----------------------------------------------------------");
    _PR_DebugPrint(out, "\n--Obj-Size----Count-----Avg-Alloc-Time-----------Avg-Lifetime---------%%Freed-\n");
    for (i = 0; i < GCSTAT_BINS; i++) {
	GCStat stat = gcstats[i];
	double allocTimeMean = 0.0, allocTimeVariance = 0.0, lifetimeMean = 0.0, lifetimeVariance = 0.0;
	PRUint32 maxSize = (1 << i);
	char* sizeStr;
	if (stat.nallocs != 0.0) {
	    allocTimeMean = stat.allocTime / stat.nallocs;
	    allocTimeVariance = fabs(stat.allocTimeVariance / stat.nallocs - allocTimeMean * allocTimeMean);
	}
	if (stat.nfrees != 0.0) {
	    lifetimeMean = stat.lifetime / stat.nfrees;
	    lifetimeVariance = fabs(stat.lifetimeVariance / stat.nfrees - lifetimeMean * lifetimeMean);
	}
	sizeStr = pr_GetSizeString(maxSize);
	_PR_DebugPrint(out, "%10s %8lu %10.3f +- %10.3f %10.3f +- %10.3f (%2ld%%)\n",
		       sizeStr, stat.nallocs,
		       allocTimeMean, sqrt(allocTimeVariance),
		       lifetimeMean, sqrt(lifetimeVariance),
		       (stat.nallocs ? (stat.nfrees * 100 / stat.nallocs) : 0));
	pr_FreeSizeString(sizeStr);
    }
    _PR_DebugPrint(out, "--Lifetime-Frequency-Counts----------------------------------------------------\n");
    _PR_DebugPrint(out, "size\\cnt");
    for (j = 0; j < GCLTFREQ_BINS; j++) {
	_PR_DebugPrint(out, "\t%lu", j);
    }
    _PR_DebugPrint(out, "\n");
    for (i = 0; i < GCSTAT_BINS; i++) {
	PRInt32* freqs = gcltfreq[i];
	_PR_DebugPrint(out, "%lu", (1 << i));
	for (j = 0; j < GCLTFREQ_BINS; j++) {
	    _PR_DebugPrint(out, "\t%lu", freqs[j]);
	}
	_PR_DebugPrint(out, "\n");
    }
    _PR_DebugPrint(out, "-------------------------------------------------------------------------------\n");
}

PR_PUBLIC_API(void)
PR_PrintGCAllocStats(void)
{
    pr_PrintGCAllocStats(stderr);
}

#endif /* GC_STATS */

/************************************************************************/

/*
** Sweep a segment, cleaning up all of the debris. Coallese the debris
** into GCFreeChunk's which are added to the freelist bins.
*/
static PRBool SweepSegment(GCSeg *sp)
{
    PRWord h, tix;
    PRWord *p;
    PRWord *np;
    PRWord *limit;
    GCFreeChunk *cp;
    PRInt32 bytes, chunkSize, segmentSize, totalFree;
    CollectorType *ct;
    PRInt32 bin;

    /*
    ** Now scan over the segment's memory in memory order, coallescing
    ** all of the debris into a FreeChunk list.
    */
    totalFree = 0;
    segmentSize = sp->limit - sp->base;
    p = (PRWord *) sp->base;
    limit = (PRWord *) sp->limit;
    PR_ASSERT(segmentSize > 0);
    while (p < limit) {
    chunkSize = 0;
    cp = (GCFreeChunk *) p;

    /* Attempt to coallesce any neighboring free objects */
    for (;;) {
        PR_ASSERT(IS_HBIT(sp, p) != 0);
        h = p[0];
        bytes = OBJ_BYTES(h);
        PR_ASSERT(bytes != 0);
        np = (PRWord *) ((char *)p + bytes);
        tix = (PRWord)GET_TYPEIX(h);
        if ((h & MARK_BIT) && (tix != FREE_MEMORY_TYPEIX)) {
#ifdef DEBUG
        if (tix != FREE_MEMORY_TYPEIX) {
            PR_ASSERT(_pr_collectorTypes[tix].flags != 0);
        }
#endif
        p[0] = h & ~(MARK_BIT|FINAL_BIT);
		_GCTRACE(GC_SWEEP, ("busy 0x%x (%d)", p, bytes));
		break;
	    }
	    _GCTRACE(GC_SWEEP, ("free 0x%x (%d)", p, bytes));

	    /* Found a free object */
#ifdef GC_STATS
	    {
		PRInt32 userSize = bytes - sizeof(GCBlockEnd);
		GCBlockEnd* end = (GCBlockEnd*)((char*)p + userSize);
		if (userSize >= 0 && end->check == PR_BLOCK_END) {
		    PRInt64 now = PR_Now();
		    double nowd, delta;
		    PRInt32 freq;
		    LL_L2D(nowd, now);
		    delta = nowd - end->allocTime;
		    gcstats[end->bin].nfrees++;
		    gcstats[end->bin].lifetime += delta;
		    gcstats[end->bin].lifetimeVariance += delta * delta;

		    InlineBinNumber(freq, delta);
		    gcltfreq[end->bin][freq]++;

		    end->check = 0;
		}
	    }
#endif
        CLEAR_HBIT(sp, p);
        ct = &_pr_collectorTypes[tix];
        if (0 != ct->gctype.free) {
                (*ct->gctype.free)(p + 1);
            }
        chunkSize = chunkSize + bytes;
        if (np == limit) {
        /* Found the end of heap */
        break;
        }
        PR_ASSERT(np < limit);
        p = np;
    }

    if (chunkSize) {
        _GCTRACE(GC_SWEEP, ("free chunk 0x%p to 0x%p (%d)",
                   cp, (char*)cp + chunkSize - 1, chunkSize));
        if (chunkSize < MIN_FREE_CHUNK_BYTES) {
        /* Lost a tiny fragment until (maybe) next time */
                METER(meter.wastedBytes += chunkSize);
        p = (PRWord *) cp;
        chunkSize >>= BYTES_PER_WORD_LOG2;
        PR_ASSERT(chunkSize != 0);
        p[0] = MAKE_HEADER(FREE_MEMORY_TYPEIX, chunkSize);
        SET_HBIT(sp, p);
        } else {
                /* See if the chunk constitutes the entire segment */
                if (chunkSize == segmentSize) {
                    /* Free up the segment right now */
            if (sp->info->fromMalloc) {
                    ShrinkGCHeap(sp);
                    return PR_TRUE;
                }
                }

                /* Put free chunk into the appropriate bin */
                cp->segment = sp;
        cp->chunkSize = chunkSize;
                InlineBinNumber(bin, chunkSize)
                cp->next = bins[bin];
                bins[bin] = cp;
        if (bin < minBin) minBin = bin;
        if (bin > maxBin) maxBin = bin;

        /* Zero swept memory now */
        memset(cp+1, 0, chunkSize - sizeof(*cp));
                METER(meter.numFreeChunks++);
        totalFree += chunkSize;
        }
    }

    /* Advance to next object */
    p = np;
    }

    PR_ASSERT(totalFree <= segmentSize);

    _pr_gcData.freeMemory += totalFree;
    _pr_gcData.busyMemory += (sp->limit - sp->base) - totalFree;
    return PR_FALSE;
}

/************************************************************************/

/* This is a list of all the objects that are finalizable. This is not
   the list of objects that are awaiting finalization because they
   have been collected. */
PRCList _pr_finalizeableObjects;

/* This is the list of objects that are awaiting finalization because
   they have been collected. */
PRCList _pr_finalQueue;

/* Each object that requires finalization has one of these objects
   allocated as well. The GCFinal objects are put on the
   _pr_finalizeableObjects list until the object is collected at which
   point the GCFinal object is moved to the _pr_finalQueue */
typedef struct GCFinalStr {
    PRCList links;
    PRWord *object;
} GCFinal;

/* Find pointer to GCFinal struct from the list linkaged embedded in it */
#define FinalPtr(_qp) \
    ((GCFinal*) ((char*) (_qp) - offsetof(GCFinal,links)))

static GCFinal *AllocFinalNode(void)
{
    return PR_NEWZAP(GCFinal);
}

static void FreeFinalNode(GCFinal *node)
{
    PR_DELETE(node);
}

/*
** Prepare for finalization. At this point in the GC cycle we have
** identified all of the live objects. For each object on the
** _pr_finalizeableObjects list see if the object is alive or dead. If
** it's dead, resurrect it and move it from the _pr_finalizeableObjects
** list to the _pr_finalQueue (object's only get finalized once).
**
** Once _pr_finalizeableObjects has been processed we can finish the
** GC and free up memory and release the threading lock. After that we
** can invoke the finalization procs for each object that is on the
** _pr_finalQueue.
*/
static void PrepareFinalize(void)
{
    PRCList *qp;
    GCFinal *fp;
    PRWord h;
    PRWord *p;
    void (PR_CALLBACK *livePointer)(void *ptr);
#ifdef DEBUG
    CollectorType *ct;
#endif

    /* This must be done under the same lock that the finalizer uses */
    PR_ASSERT( GC_IS_LOCKED() );

    /* cache this ptr */
    livePointer = _pr_gcData.livePointer;

    /*
     * Pass #1: Identify objects that are to be finalized, set their
     * FINAL_BIT.
     */
    qp = _pr_finalizeableObjects.next;
    while (qp != &_pr_finalizeableObjects) {
    fp = FinalPtr(qp);
    qp = qp->next;
    h = fp->object[0];        /* Grab header word */
    if (h & MARK_BIT) {
        /* Object is already alive */
        continue;
    }

#ifdef DEBUG
    ct = &_pr_collectorTypes[GET_TYPEIX(h)];
    PR_ASSERT((0 != ct->flags) && (0 != ct->gctype.finalize));
#endif
    fp->object[0] |= FINAL_BIT;
    _GCTRACE(GC_FINAL, ("moving %p (%d) to finalQueue",
               fp->object, OBJ_BYTES(h)));
    }

    /*
     * Pass #2: For each object that is going to be finalized, move it to
     * the finalization queue and resurrect it
     */
    qp = _pr_finalizeableObjects.next;
    while (qp != &_pr_finalizeableObjects) {
    fp = FinalPtr(qp);
    qp = qp->next;
    h = fp->object[0];        /* Grab header word */
    if ((h & FINAL_BIT) == 0) {
        continue;
    }

    /* Resurrect the object and any objects it refers to */
        p = &fp->object[1];
    (*livePointer)(p);
    PR_REMOVE_LINK(&fp->links);
    PR_APPEND_LINK(&fp->links, &_pr_finalQueue);
    }
}

/*
** Scan the finalQ, marking each and every object on it live.  This is
** necessary because we might do a GC before objects that are on the
** final queue get finalized. Since there are no other references
** (otherwise they would be on the final queue), we have to scan them.
** This really only does work if we call the GC before the finalizer
** has a chance to do its job.
*/
extern void PR_CALLBACK _PR_ScanFinalQueue(void *notused)
{
#ifdef XP_MAC
#pragma unused (notused)
#endif
    PRCList *qp;
    GCFinal *fp;
    PRWord *p;
    void ( PR_CALLBACK *livePointer)(void *ptr);

    livePointer = _pr_gcData.livePointer;
    qp = _pr_finalQueue.next;
    while (qp != &_pr_finalQueue) {
    fp = FinalPtr(qp);
	_GCTRACE(GC_FINAL, ("marking 0x%x (on final queue)", fp->object));
        p = &fp->object[1];
    (*livePointer)(p);
    qp = qp->next;
    }
}

void PR_CALLBACK FinalizerLoop(void* unused)
{
#ifdef XP_MAC
#pragma unused (unused)
#endif
    GCFinal *fp;
    PRWord *p;
    PRWord h, tix;
    CollectorType *ct;

    LOCK_GC();
    for (;;) {
	p = 0; h = 0;		/* don't let the gc find these pointers */
    while (PR_CLIST_IS_EMPTY(&_pr_finalQueue))
        PR_Wait(_pr_gcData.lock, PR_INTERVAL_NO_TIMEOUT);

    _GCTRACE(GC_FINAL, ("begin finalization"));
    while (_pr_finalQueue.next != &_pr_finalQueue) {
        fp = FinalPtr(_pr_finalQueue.next);
        PR_REMOVE_LINK(&fp->links);
        p = fp->object;

        h = p[0];        /* Grab header word */
        tix = (PRWord)GET_TYPEIX(h);
        ct = &_pr_collectorTypes[tix];
	    _GCTRACE(GC_FINAL, ("finalize 0x%x (%d)", p, OBJ_BYTES(h)));

        /*
        ** Give up the GC lock so that other threads can allocate memory
        ** while this finalization method is running. Get it back
        ** afterwards so that the list remains thread safe.
        */
        UNLOCK_GC();
        FreeFinalNode(fp);
        PR_ASSERT(ct->gctype.finalize != 0);
        (*ct->gctype.finalize)(p + 1);
        LOCK_GC();
    }
    _GCTRACE(GC_FINAL, ("end finalization"));
    PR_Notify(_pr_gcData.lock);
    }
}

static void NotifyFinalizer(void)
{
    if (!PR_CLIST_IS_EMPTY(&_pr_finalQueue)) {
    PR_ASSERT( GC_IS_LOCKED() );
    PR_Notify(_pr_gcData.lock);
    }
}

void _PR_CreateFinalizer(PRThreadScope scope)
{
    if (!_pr_gcData.finalizer) {
    _pr_gcData.finalizer = PR_CreateThreadGCAble(PR_SYSTEM_THREAD,
                                        FinalizerLoop, 0,
                                        PR_PRIORITY_LOW, scope,
                                        PR_UNJOINABLE_THREAD, 0);
    
    if (_pr_gcData.finalizer == NULL)
        /* We are doomed if we can't start the finalizer */
        PR_Abort();

    }
}

void pr_FinalizeOnExit(void)
{
#ifdef DEBUG_warren
    OutputDebugString("### Doing finalize-on-exit pass\n");
#endif
    PR_ForceFinalize();
#ifdef DEBUG_warren
    OutputDebugString("### Finalize-on-exit complete. Dumping object left to memory.out\n");
    PR_DumpMemorySummary();
    PR_DumpMemory(PR_TRUE);
#endif
}

PR_IMPLEMENT(void) PR_ForceFinalize()
{
    LOCK_GC();
    NotifyFinalizer();
    while (!PR_CLIST_IS_EMPTY(&_pr_finalQueue)) {
    PR_ASSERT( GC_IS_LOCKED() );
    (void) PR_Wait(_pr_gcData.lock, PR_INTERVAL_NO_TIMEOUT);
    }
    UNLOCK_GC();

    /* XXX I don't know how to make it wait (yet) */
}

/************************************************************************/

typedef struct GCWeakStr {
    PRCList links;
    PRWord *object;
} GCWeak;

/*
** Find pointer to GCWeak struct from the list linkaged embedded in it
*/
#define WeakPtr(_qp) \
    ((GCWeak*) ((char*) (_qp) - offsetof(GCWeak,links)))

PRCList _pr_weakLinks = PR_INIT_STATIC_CLIST(&_pr_weakLinks);
PRCList _pr_freeWeakLinks = PR_INIT_STATIC_CLIST(&_pr_freeWeakLinks);

#define WEAK_FREELIST_ISEMPTY() (_pr_freeWeakLinks.next == &_pr_freeWeakLinks)

/*
 * Keep objects referred to by weak free list alive until they can be
 * freed
 */
static void PR_CALLBACK ScanWeakFreeList(void *notused) {
#ifdef XP_MAC
#pragma unused (notused)
#endif
    PRCList *qp = _pr_freeWeakLinks.next;
    while (qp != &_pr_freeWeakLinks) {
    GCWeak *wp = WeakPtr(qp);
    qp = qp->next;
    ProcessRootPointer(wp->object);
    }
}

/*
 * Empty the list of weak objects. Note that we can't call malloc/free
 * under the cover of the GC's lock (we might deadlock), so transfer the
 * list of free objects to a local list under the cover of the lock, then
 * release the lock and free up the memory.
 */
static void EmptyWeakFreeList(void) {
    if (!WEAK_FREELIST_ISEMPTY()) {
    PRCList *qp, freeLinks;

    PR_INIT_CLIST(&freeLinks);

    /*
     * Transfer list of free weak links from the global list to a
     * local list.
     */
    LOCK_GC();
    qp = _pr_freeWeakLinks.next;
    while (qp != &_pr_freeWeakLinks) {
        GCWeak *wp = WeakPtr(qp);
        qp = qp->next;
        PR_REMOVE_LINK(&wp->links);
        PR_APPEND_LINK(&wp->links, &freeLinks);
    }
    UNLOCK_GC();

    /* Free up storage now */
    qp = freeLinks.next;
    while (qp != &freeLinks) {
        GCWeak *wp = WeakPtr(qp);
        qp = qp->next;
        PR_DELETE(wp);
    }
    }
}

/*
 * Allocate a new weak node in the weak objects list
 */
static GCWeak *AllocWeakNode(void)
{
    EmptyWeakFreeList();
    return PR_NEWZAP(GCWeak);
}

static void FreeWeakNode(GCWeak *node)
{
    PR_DELETE(node);
}

/*
 * Check the weak links for validity. Note that the list of weak links is
 * itself weak (otherwise we would keep the objects with weak links in
 * them alive forever). As we scan the list check the weak link object
 * itself and if it's not marked then remove it from the weak link list
 */
static void CheckWeakLinks(void) {
    PRCList *qp;
    GCWeak *wp;
    PRWord *p, h, tix, **weakPtrAddress;
    CollectorType *ct;
    PRUint32 offset;

    qp = _pr_weakLinks.next;
    while (qp != &_pr_weakLinks) {
    wp = WeakPtr(qp);
    qp = qp->next;
    if ((p = wp->object) != 0) {
        h = p[0];        /* Grab header word */
        if ((h & MARK_BIT) == 0) {
        /*
         * The object that has a weak link is no longer being
         * referenced; remove it from the chain and let it get
         * swept away by the GC. Transfer it to the list of
         * free weak links for later freeing.
         */
        PR_REMOVE_LINK(&wp->links);
        PR_APPEND_LINK(&wp->links, &_pr_freeWeakLinks);
        collectorCleanupNeeded = 1;
        continue;
        }
        
	    /* Examine a live object that contains weak links */
        tix = GET_TYPEIX(h);
        ct = &_pr_collectorTypes[tix];
        PR_ASSERT((ct->flags != 0) && (ct->gctype.getWeakLinkOffset != 0));
        if (0 == ct->gctype.getWeakLinkOffset) {
        /* Heap is probably corrupted */
        continue;
        }

        /* Get offset into the object of where the weak pointer is */
        offset = (*ct->gctype.getWeakLinkOffset)(p + 1);

        /* Check the weak pointer */
        weakPtrAddress = (PRWord**)((char*)(p + 1) + offset);
        p = *weakPtrAddress;
        if (p != 0) {
        h = p[-1];    /* Grab header word for pointed to object */
        if (h & MARK_BIT) {
            /* Object can't be dead */
            continue;
        }
        /* Break weak link to an object that is about to be swept */
        *weakPtrAddress = 0;
        }
    }
    }
}

/************************************************************************/

/*
** Perform a complete garbage collection
*/

extern GCLockHook *_pr_GCLockHook;

static void dogc(void)
{
    RootFinder *rf;
    GCLockHook* lhook;

    GCScanQ scanQ;
    GCSeg *sp, *esp;
    PRInt64 start, end, diff;

#if defined(GCMETER) || defined(GCTIMINGHOOK)
    start = PR_Now();
#endif

    /*
    ** Stop all of the other threads. This also promises to capture the
    ** register state of each and every thread
    */

    /* 
    ** Get all the locks that will be need during GC after SuspendAll. We 
    ** cannot make any locking/library calls after SuspendAll.
    */
    if (_pr_GCLockHook) {
        for (lhook = _pr_GCLockHook->next; lhook != _pr_GCLockHook; 
          lhook = lhook->next) {
          (*lhook->func)(PR_GCBEGIN, lhook->arg);
        }
    }

    PR_SuspendAll();

#ifdef GCMETER
    /* Reset meter info */
    if (_pr_gcMeter & _GC_METER_STATS) {
        fprintf(stderr,
                "[GCSTATS: busy:%ld skipped:%ld, alloced:%ld+wasted:%ld+free:%ld = total:%ld]\n",
                (long) _pr_gcData.busyMemory,
                (long) meter.skippedFreeChunks,
                (long) meter.allocBytes,
                (long) meter.wastedBytes,
                (long) _pr_gcData.freeMemory,
                (long) _pr_gcData.allocMemory);
    }        
    memset(&meter, 0, sizeof(meter));
#endif

    PR_LOG(_pr_msgc_lm, PR_LOG_ALWAYS, ("begin mark phase; busy=%d free=%d total=%d",
                     _pr_gcData.busyMemory, _pr_gcData.freeMemory,
                     _pr_gcData.allocMemory));

    if (_pr_beginGCHook) {
    (*_pr_beginGCHook)(_pr_beginGCHookArg);
    }

    /*
    ** Initialize scanQ to all zero's so that root finder doesn't walk
    ** over it...
    */
    memset(&scanQ, 0, sizeof(scanQ));
    pScanQ = &scanQ;

    /******************************************/
    /* MARK PHASE */

    EmptyFreelists();

    /* Find root's */
    PR_LOG(_pr_msgc_lm, PR_LOG_WARNING,
           ("begin mark phase; busy=%d free=%d total=%d",
        _pr_gcData.busyMemory, _pr_gcData.freeMemory,
            _pr_gcData.allocMemory));
    METER(_pr_scanDepth = 0);
    rf = _pr_rootFinders;
    while (rf) {
    _GCTRACE(GC_ROOTS, ("finding roots in %s", rf->name));
    (*rf->func)(rf->arg);
    rf = rf->next;
    }
    _GCTRACE(GC_ROOTS, ("done finding roots"));

    /* Scan remaining object's that need scanning */
    ScanScanQ(&scanQ);
    PR_ASSERT(pScanQ == &scanQ);
    PR_ASSERT(scanQ.queued == 0);
    METER({
    if (_pr_scanDepth > _pr_maxScanDepth) {
        _pr_maxScanDepth = _pr_scanDepth;
    }
    });

    /******************************************/
    /* FINALIZATION PHASE */

    METER(_pr_scanDepth = 0);
    PrepareFinalize();

    /* Scan any resurrected objects found during finalization */
    ScanScanQ(&scanQ);
    PR_ASSERT(pScanQ == &scanQ);
    PR_ASSERT(scanQ.queued == 0);
    METER({
    if (_pr_scanDepth > _pr_maxScanDepth) {
        _pr_maxScanDepth = _pr_scanDepth;
    }
    });
    pScanQ = 0;

    /******************************************/
    /* SWEEP PHASE */

    /*
    ** Sweep each segment clean. While we are at it, figure out which
    ** segment has the most free space and make that the current segment.
    */
    CheckWeakLinks();
    _GCTRACE(GC_SWEEP, ("begin sweep phase"));
    _pr_gcData.freeMemory = 0;
    _pr_gcData.busyMemory = 0;
    sp = segs;
    esp = sp + nsegs;
    while (sp < esp) {
        if (SweepSegment(sp)) {
            /*
            ** Segment is now free and has been replaced with a different
            ** segment object.
            */
            esp--;
            continue;
        }
        sp++;
    }

#if defined(GCMETER) || defined(GCTIMINGHOOK)
    end = PR_Now();
#endif
#ifdef GCMETER
    LL_SUB(diff, end, start);
    PR_LOG(GC, PR_LOG_ALWAYS,
	   ("done; busy=%d free=%d chunks=%d total=%d time=%lldms",
	    _pr_gcData.busyMemory, _pr_gcData.freeMemory,
	    meter.numFreeChunks, _pr_gcData.allocMemory, diff));
    if (_pr_gcMeter & _GC_METER_FREE_LIST) {
        PRIntn bin;
        fprintf(stderr, "Freelist bins:\n");
        for (bin = 0; bin < NUM_BINS; bin++) {
            GCFreeChunk *cp = bins[bin];
            while (cp != NULL) {
                fprintf(stderr, "%3d: %p %8ld\n",
                        bin, cp, (long) cp->chunkSize);
                cp = cp->next;
            }
        }
    }
#endif

    if (_pr_endGCHook) {
    (*_pr_endGCHook)(_pr_endGCHookArg);
    }

    /* clear the running total of the bytes allocated via BigAlloc() */
    bigAllocBytes = 0;

    /* And resume multi-threading */
    PR_ResumeAll();

    if (_pr_GCLockHook) {
        for (lhook = _pr_GCLockHook->prev; lhook != _pr_GCLockHook; 
          lhook = lhook->prev) {
          (*lhook->func)(PR_GCEND, lhook->arg);
        }
    }

    /* Kick finalizer */
    NotifyFinalizer();
#ifdef GCTIMINGHOOK
    if (_pr_gcData.gcTimingHook) {
	PRInt32 time;
	LL_SUB(diff, end, start);
	LL_L2I(time, diff);
	_pr_gcData.gcTimingHook(time);
    }
#endif
}

PR_IMPLEMENT(void) PR_GC(void)
{
    LOCK_GC();
    dogc();
    UNLOCK_GC();

    EmptyWeakFreeList();
}

/*******************************************************************************
 * Heap Walker
 ******************************************************************************/

/*
** This is yet another disgusting copy of the body of ProcessRootPointer
** (the other being ProcessRootBlock), but we're not leveraging a single
** function in their cases in interest of performance (avoiding the function
** call).
*/
static PRInt32 PR_CALLBACK
pr_ConservativeWalkPointer(void* ptr, PRWalkFun walkRootPointer, void* data)
{
  PRWord *p0, *p, *segBase;
  GCSeg* sp;

  p0 = (PRWord*) ptr;

  /*
  ** XXX:  
  ** Until Win16 maintains lowSeg and highSeg correctly,
  ** (ie. lowSeg=MIN(all segs) and highSeg = MAX(all segs))
  ** Allways scan through the segment list
  */
#if !defined(WIN16)
  if (p0 < _pr_gcData.lowSeg) return 0;                  /* below gc heap */
  if (p0 >= _pr_gcData.highSeg) return 0;                /* above gc heap */
#endif

  /* NOTE: inline expansion of InHeap */
  /* Find segment */
  sp = lastInHeap;
  if (!sp || !IN_SEGMENT(sp,p0)) {
    GCSeg *esp;
    sp = segs;
    esp = segs + nsegs;
    for (; sp < esp; sp++) {
      if (IN_SEGMENT(sp, p0)) {
	lastInHeap = sp;
	goto find_object;
      }
    }
    return 0;
  }

  find_object:
    /* NOTE: Inline expansion of FindObject */
    /* Align p to it's proper boundary before we start fiddling with it */
    p = (PRWord*) ((PRWord)p0 & ~(BYTES_PER_WORD-1L));
    segBase = (PRWord *) sp->base;
    do {
        if (IS_HBIT(sp, p)) {
            goto winner;
        }
        p--;
    } while (p >= segBase);

    /*
    ** We have a pointer into the heap, but it has no header
    ** bit. This means that somehow the very first object in the heap
    ** doesn't have a header. This is impossible so when debugging
    ** lets abort.
    */
#ifdef DEBUG
    PR_Abort();
#endif
    return 0;

 winner:
    return walkRootPointer(p, data);
}

static PRInt32 PR_CALLBACK
pr_ConservativeWalkBlock(void **base, PRInt32 count,
			 PRWalkFun walkRootPointer, void* data)
{
    PRWord *p0;
    while (--count >= 0) {
	PRInt32 status;
        p0 = (PRWord*) *base++;
	status = pr_ConservativeWalkPointer(p0, walkRootPointer, data);
	if (status) return status;
    }
    return 0;
}

/******************************************************************************/

typedef void (*WalkObject_t)(FILE *out, GCType* tp, PRWord *obj,
			     size_t bytes, PRBool detailed);
typedef void (*WalkUnknown_t)(FILE *out, GCType* tp, PRWord tix, PRWord *p,
			      size_t bytes, PRBool detailed);
typedef void (*WalkFree_t)(FILE *out, PRWord *p, size_t size, PRBool detailed);
typedef void (*WalkSegment_t)(FILE *out, GCSeg* sp, PRBool detailed);

static void
pr_WalkSegment(FILE* out, GCSeg* sp, PRBool detailed,
           char* enterMsg, char* exitMsg,
           WalkObject_t walkObject, WalkUnknown_t walkUnknown, WalkFree_t walkFree)
{
    PRWord *p, *limit;

    p = (PRWord *) sp->base;
    limit = (PRWord *) sp->limit;
    if (enterMsg)
    fprintf(out, enterMsg, p);
    while (p < limit)
    {
    if (IS_HBIT(sp, p)) /* Is this an object header? */
    {
        PRWord h = p[0];
        PRWord tix = GET_TYPEIX(h);
        size_t bytes = OBJ_BYTES(h);
        PRWord* np = (PRWord*) ((char*)p + bytes);

        GCType* tp = &_pr_collectorTypes[tix].gctype;
        if ((0 != tp) && walkObject)
        walkObject(out, tp, p, bytes, detailed);
        else if (walkUnknown)
        walkUnknown(out, tp, tix, p, bytes, detailed);
        p = np;
    }
    else
    {
        /* Must be a freelist item */
        size_t size = ((GCFreeChunk*)p)->chunkSize;
        if (walkFree)
        walkFree(out, p, size, detailed);
        p = (PRWord*)((char*)p + size);
    }
    }
    if (p != limit)
    fprintf(out, "SEGMENT OVERRUN (end should be at 0x%p)\n", limit);
    if (exitMsg)
    fprintf(out, exitMsg, p);
}

static void
pr_WalkSegments(FILE *out, WalkSegment_t walkSegment, PRBool detailed)
{
    GCSeg *sp = segs;
    GCSeg *esp;

    LOCK_GC();
    esp = sp + nsegs;
    while (sp < esp)
    {
    walkSegment(out, sp, detailed);
    sp++;
    }
    fprintf(out, "End of heap\n");
    UNLOCK_GC();
}

/*******************************************************************************
 * Heap Dumper
 ******************************************************************************/

PR_IMPLEMENT(void)
PR_DumpIndent(FILE *out, int indent)
{
    while (--indent >= 0)
    fprintf(out, " ");
}

static void
PR_DumpHexWords(FILE *out, PRWord *p, int nWords,
        int indent, int nWordsPerLine)
{
    while (nWords > 0)
    {
    int i;

    PR_DumpIndent(out, indent);
    i = nWordsPerLine;
    if (i > nWords)
        i = nWords;
    nWords -= i;
    while (i--)
    {
        fprintf(out, "0x%.8lX", (long) *p++);
        if (i)
        fputc(' ', out);
    }
    fputc('\n', out);
    }
}

static void PR_CALLBACK
pr_DumpObject(FILE *out, GCType* tp, PRWord *p, 
          size_t bytes, PRBool detailed)
{
    char kindChar = tp->kindChar;
    fprintf(out, "0x%p: 0x%.6lX %c  ",
            p, (long) bytes, kindChar ? kindChar : '?');
    if (tp->dump)
    (*tp->dump)(out, (void*) (p + 1), detailed, 0);
    if (detailed)
    PR_DumpHexWords(out, p, bytes>>2, 22, 4);
}
    
static void PR_CALLBACK
pr_DumpUnknown(FILE *out, GCType* tp, PRWord tix, PRWord *p, 
           size_t bytes, PRBool detailed)
{
    char kindChar = tp->kindChar;
    fprintf(out, "0x%p: 0x%.6lX %c  ",
            p, (long) bytes, kindChar ? kindChar : '?');
    fprintf(out, "UNKNOWN KIND %ld\n", (long) tix);
    if (detailed)
    PR_DumpHexWords(out, p, bytes>>2, 22, 4);
}

static void PR_CALLBACK
pr_DumpFree(FILE *out, PRWord *p, size_t size, PRBool detailed)
{
#if defined(XP_MAC) && XP_MAC
# pragma unused( detailed )
#endif

    fprintf(out, "0x%p: 0x%.6lX -  FREE\n", p, (long) size);
}

static void PR_CALLBACK
pr_DumpSegment(FILE* out, GCSeg* sp, PRBool detailed)
{
    pr_WalkSegment(out, sp, detailed,
           "\n   Address: Length\n0x%p: Beginning of segment\n",
           "0x%p: End of segment\n\n",
           pr_DumpObject, pr_DumpUnknown, pr_DumpFree);
}

static void pr_DumpRoots(FILE *out);

/*
** Dump out the GC heap.
*/
PR_IMPLEMENT(void)
PR_DumpGCHeap(FILE *out, PRBool detailed)
{
    fprintf(out, "\n"
        "The kinds are:\n"
        " U unscanned block\n"
        " W weak link block\n"
        " S scanned block\n"
        " F scanned and final block\n"
        " C class record\n"
        " X context record\n"
        " - free list item\n"
        " ? other\n");
    LOCK_GC();
    pr_WalkSegments(out, pr_DumpSegment, detailed);
    if (detailed)
    pr_DumpRoots(out);
    UNLOCK_GC();
}

PR_IMPLEMENT(void)
PR_DumpMemory(PRBool detailed)
{
    PR_DumpToFile("memory.out", "Dumping memory", PR_DumpGCHeap, detailed);
}

/******************************************************************************/

static PRInt32 PR_CALLBACK
pr_DumpRootPointer(PRWord* p, void* data)
{
#ifdef XP_MAC
#pragma unused(data)
#endif
    PRWord h = p[0];
    PRWord tix = GET_TYPEIX(h);
      size_t bytes = OBJ_BYTES(h);
      
      GCType* tp = &_pr_collectorTypes[tix].gctype;
      if (0 != tp)
      pr_DumpObject(_pr_gcData.dumpOutput, tp, p, bytes, PR_FALSE);
      else
      pr_DumpUnknown(_pr_gcData.dumpOutput, tp, tix, p, bytes, PR_FALSE);
    return 0;
}

static void PR_CALLBACK
pr_ConservativeDumpRootPointer(void* ptr)
{
    (void)pr_ConservativeWalkPointer(ptr, (PRWalkFun) pr_DumpRootPointer, NULL);
}

static void PR_CALLBACK
pr_ConservativeDumpRootBlock(void **base, PRInt32 count)
{
    (void)pr_ConservativeWalkBlock(base, count, (PRWalkFun) pr_DumpRootPointer, NULL);
}

extern int
DumpThreadRoots(PRThread *t, int i, void *notused);

static void
pr_DumpRoots(FILE *out)
{
    RootFinder *rf;
    void (*liveBlock)(void **base, PRInt32 count);
    void (*livePointer)(void *ptr);
    void (*processRootBlock)(void **base, PRInt32 count);
    void (*processRootPointer)(void *ptr);

    LOCK_GC();

    liveBlock = _pr_gcData.liveBlock;
    livePointer = _pr_gcData.livePointer;
    processRootBlock = _pr_gcData.processRootBlock;
    processRootPointer = _pr_gcData.processRootPointer;
    
    _pr_gcData.liveBlock = pr_ConservativeDumpRootBlock;
    _pr_gcData.livePointer = pr_ConservativeDumpRootPointer;
    _pr_gcData.processRootBlock = pr_ConservativeDumpRootBlock;
    _pr_gcData.processRootPointer = pr_ConservativeDumpRootPointer;
    _pr_gcData.dumpOutput = out;

    rf = _pr_rootFinders;
    while (rf) {
    fprintf(out, "\n===== Roots for %s\n", rf->name);
    (*rf->func)(rf->arg);
    rf = rf->next;
    }

    _pr_gcData.liveBlock = liveBlock;
    _pr_gcData.livePointer = livePointer;
    _pr_gcData.processRootBlock = processRootBlock;
    _pr_gcData.processRootPointer = processRootPointer;
    _pr_gcData.dumpOutput = NULL;

    UNLOCK_GC();
}

/*******************************************************************************
 * Heap Summary Dumper
 ******************************************************************************/

PRSummaryPrinter summaryPrinter = NULL;
void* summaryPrinterClosure = NULL;

PR_IMPLEMENT(void) 
PR_RegisterSummaryPrinter(PRSummaryPrinter fun, void* closure)
{
    summaryPrinter = fun;
    summaryPrinterClosure = closure;
}

static void PR_CALLBACK
pr_SummarizeObject(FILE *out, GCType* tp, PRWord *p,
           size_t bytes, PRBool detailed)
{
#if defined(XP_MAC) && XP_MAC
# pragma unused( out, detailed )
#endif

    if (tp->summarize)
    (*tp->summarize)((void GCPTR*)(p + 1), bytes);
}

static void PR_CALLBACK
pr_DumpSummary(FILE* out, GCSeg* sp, PRBool detailed)
{
    pr_WalkSegment(out, sp, detailed, NULL, NULL,
           pr_SummarizeObject, NULL, NULL);
}

PR_IMPLEMENT(void)
PR_DumpGCSummary(FILE *out, PRBool detailed)
{
    if (summaryPrinter) {
    pr_WalkSegments(out, pr_DumpSummary, detailed);
    summaryPrinter(out, summaryPrinterClosure);
    }
#if 0
    fprintf(out, "\nFinalizable objects:\n");
    {
    PRCList *qp;
    qp = _pr_pendingFinalQueue.next;
    while (qp != &_pr_pendingFinalQueue) {
        GCFinal* fp = FinalPtr(qp);
        PRWord h = fp->object[0];        /* Grab header word */
        PRWord tix = GET_TYPEIX(h);
        GCType* tp = _pr_gcTypes[tix];
        size_t bytes = OBJ_BYTES(h);
        pr_DumpObject(out, tp, fp->object, bytes, PR_FALSE);
        qp = qp->next;
    }
    }
#endif
}

PR_IMPLEMENT(void)
PR_DumpMemorySummary(void)
{
    PR_DumpToFile("memory.out", "Memory Summary", PR_DumpGCSummary, PR_FALSE);
}

/*******************************************************************************
 * End Of Heap Walker 
 ******************************************************************************/

#ifdef GC_TRACEROOTS

PRInt32 pr_traceGen = 0;

static PRBool
pr_IsMarked(PRWord* p)
{
    GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));
    PR_ASSERT(end->check == PR_BLOCK_END);
    return end->traceGeneration == pr_traceGen;
}

static void
pr_Mark(PRWord* p)
{
    GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));
    PR_ASSERT(end->check == PR_BLOCK_END);
    end->traceGeneration = pr_traceGen;
}

PRWord* pr_traceObj;	/* set this in the debugger, then execute PR_TraceRoot() */

static PRInt32 PR_CALLBACK
pr_TraceRootObject(void* obj, void* data);

static PRInt32 PR_CALLBACK
pr_TraceRootPointer(PRWord *p, void* data)
{
    PRInt32 printTrace = 0;
    PRWord h = p[0];
    PRWord tix = GET_TYPEIX(h);
    GCType* tp = &_pr_collectorTypes[tix].gctype;
    FILE* out = _pr_gcData.dumpOutput;

    PR_ASSERT(tp);
    if (pr_IsMarked(p))
	return printTrace;

    pr_Mark(p);
    if (p == pr_traceObj) {
	fprintf(out, "\n### Found path to:\n");
	printTrace = 1;
    }
    else {
	if (PR_StackSpaceLeft(PR_GetCurrentThread()) < 512) {
	    fprintf(out, "\n### Path too deep (giving up):\n");
	    printTrace = 1;
	}
	else if (tp->walk) {
	    printTrace = tp->walk((void*)(p + 1), pr_TraceRootObject, data);
	}
	/* else there's no way to walk this object, so we
	   haven't found what we're looking for */
    }

    if (printTrace == 1) {
	PR_ASSERT(tp->dump);
	fprintf(out, "0x%p: ", p);
	tp->dump(out, (void*)(p + 1), PR_FALSE, 1);
    }
    return printTrace;
}

static PRInt32 PR_CALLBACK
pr_TraceRootObject(void* obj, void* data)
{
    /* This version of pr_TraceRootPointer takes object
       pointers, instead of gc header pointers. */
    return pr_TraceRootPointer((PRWord*)obj - 1, data);
}

static void PR_CALLBACK
pr_ConservativeTraceRootPointer(PRWord *p)
{
    PRInt32 status;
    ++pr_traceGen;
    status = pr_ConservativeWalkPointer(p, pr_TraceRootPointer, NULL);
    if (status) {
	FILE* out = _pr_gcData.dumpOutput;
	fprintf(out, "### from root at 0x%p\n\n", p);
    }
}

static void PR_CALLBACK
pr_ConservativeTraceRootBlock(void **base, PRInt32 count)
{
    PRInt32 status;
    ++pr_traceGen;
    status = pr_ConservativeWalkBlock(base, count, pr_TraceRootPointer, NULL);
    if (status) {
	FILE* out = _pr_gcData.dumpOutput;
	fprintf(out, "### from root in range 0x%p + 0x%lx\n\n",
                base, (long) count);
    }
}

static void
PR_TraceRoot1(FILE* out, PRBool detailed)
{
    RootFinder *rf;
    void (*liveBlock)(void **base, PRInt32 count);
    void (*livePointer)(void *ptr);
    void (*processRootBlock)(void **base, PRInt32 count);
    void (*processRootPointer)(void *ptr);

    LOCK_GC();

    liveBlock = _pr_gcData.liveBlock;
    livePointer = _pr_gcData.livePointer;
    processRootBlock = _pr_gcData.processRootBlock;
    processRootPointer = _pr_gcData.processRootPointer;
    
    _pr_gcData.liveBlock = pr_ConservativeTraceRootBlock;
    _pr_gcData.livePointer = pr_ConservativeTraceRootPointer;
    _pr_gcData.processRootBlock = pr_ConservativeTraceRootBlock;
    _pr_gcData.processRootPointer = pr_ConservativeTraceRootPointer;
    _pr_gcData.dumpOutput = out;

    fprintf(out, "### Looking for paths to 0x%p\n\n", pr_traceObj);

    rf = _pr_rootFinders;
    while (rf) {
	fprintf(out, "\n===== Roots for %s\n", rf->name);
	(*rf->func)(rf->arg);
	rf = rf->next;
    }

    _pr_gcData.liveBlock = liveBlock;
    _pr_gcData.livePointer = livePointer;
    _pr_gcData.processRootBlock = processRootBlock;
    _pr_gcData.processRootPointer = processRootPointer;
    _pr_gcData.dumpOutput = NULL;

    UNLOCK_GC();
}

PR_PUBLIC_API(void)
PR_TraceRoot()
{
    /*
    ** How this works: 
    ** Once you find the object you want to trace the roots of, set the
    ** global variable pr_traceObj to point to it (the header, not the
    ** java handle), and then call this routine (on Windows, you can set
    ** a breakpoint at the end of a function that returns void (e.g. dogc)
    ** and then do a "set next statement" to point to this routine and go.
    ** This will dump a list of the paths from the roots to the object in
    ** question to your memory.out file.
    */
    PR_DumpToFile("memory.out", "Tracing Roots", PR_TraceRoot1, PR_FALSE);
}

#endif /* GC_TRACEROOTS */

/******************************************************************************/

#if defined(DEBUG) && defined(WIN32)
static void DumpApplicationHeap(FILE *out, HANDLE heap)
{
    PROCESS_HEAP_ENTRY entry;
    DWORD err;

    if (!HeapLock(heap))
    OutputDebugString("Can't lock the heap.\n");
    entry.lpData = 0;
    fprintf(out, "   address:       size ovhd region\n");
    while (HeapWalk(heap, &entry))
    {
    WORD flags = entry.wFlags;

    fprintf(out, "0x%.8X: 0x%.8X 0x%.2X 0x%.2X  ", entry.lpData, entry.cbData,
        entry.cbOverhead, entry.iRegionIndex);
    if (flags & PROCESS_HEAP_REGION)
        fprintf(out, "REGION  committedSize=0x%.8X uncommittedSize=0x%.8X firstBlock=0x%.8X lastBlock=0x%.8X",
            entry.Region.dwCommittedSize, entry.Region.dwUnCommittedSize,
            entry.Region.lpFirstBlock, entry.Region.lpLastBlock);
    else if (flags & PROCESS_HEAP_UNCOMMITTED_RANGE)
        fprintf(out, "UNCOMMITTED");
    else if (flags & PROCESS_HEAP_ENTRY_BUSY)
    {
        if (flags & PROCESS_HEAP_ENTRY_DDESHARE)
        fprintf(out, "DDEShare ");
        if (flags & PROCESS_HEAP_ENTRY_MOVEABLE)
        fprintf(out, "Moveable Block  handle=0x%.8X", entry.Block.hMem);
        else
        fprintf(out, "Block");
    }
    fprintf(out, "\n");
    }
    if ((err = GetLastError()) != ERROR_NO_MORE_ITEMS)
    fprintf(out, "ERROR %d iterating through the heap\n", err);
    if (!HeapUnlock(heap))
    OutputDebugString("Can't unlock the heap.\n");
}
#endif

#if defined(DEBUG) && defined(WIN32)
static void DumpApplicationHeaps(FILE *out)
{
    HANDLE mainHeap;
    HANDLE heaps[100];
    DWORD nHeaps;
    PRInt32 i;

    mainHeap = GetProcessHeap();
    nHeaps = GetProcessHeaps(100, heaps);
    if (nHeaps > 100)
    nHeaps = 0;
    fprintf(out, "%ld heaps:\n", (long) nHeaps);
    for (i = 0; i<nHeaps; i++)
    {
    HANDLE heap = heaps[i];

    fprintf(out, "Heap at 0x%.8lX", (long) heap);
    if (heap == mainHeap)
        fprintf(out, " (main)");
    fprintf(out, ":\n");
    DumpApplicationHeap(out, heap);
    fprintf(out, "\n");
    }
    fprintf(out, "End of heap dump\n\n");
}
#endif

#if defined(DEBUG) && defined(WIN32)
PR_IMPLEMENT(void) PR_DumpApplicationHeaps(void)
{
    FILE *out;

    OutputDebugString("Dumping heaps...");
    out = fopen("heaps.out", "a");
    if (!out)
    OutputDebugString("Can't open \"heaps.out\"\n");
    else
    {
    struct tm *newtime;
    time_t aclock;

    time(&aclock);
    newtime = localtime(&aclock);
    fprintf(out, "Heap dump on %s\n", asctime(newtime));    /* Print current time */
    DumpApplicationHeaps(out);
    fprintf(out, "\n\n");
    fclose(out);
    }
    OutputDebugString(" done\n");
}
#else

PR_IMPLEMENT(void) PR_DumpApplicationHeaps(void)
{
    fprintf(stderr, "Native heap dumping is currently implemented only for Windows32.\n");
}
#endif

/************************************************************************/

/*
** Scan the freelist bins looking for a big enough chunk of memory to
** hold "bytes" worth of allocation. "bytes" already has the
** per-allocation header added to it. Return a pointer to the object with
** its per-allocation header already prepared.
*/
static PRWord *BinAlloc(int cbix, PRInt32 bytes, int dub)
{
    GCFreeChunk **cpp, *cp, *cpNext;
    GCSeg *sp;
    PRInt32 chunkSize, remainder;
    PRWord *p, *np;
    PRInt32 bin, newbin;

    /* Compute bin that allocation belongs in */
    InlineBinNumber(bin,bytes)
    if (bin < minBin) {
    bin = minBin;    /* Start at first filled bin */
    }

    /* Search in the bin, and larger bins, for a big enough piece */
    for (; bin <= NUM_BINS-1; bin++) {
        cpp = &bins[bin];
    while ((cp = *cpp) != 0) {
        chunkSize = cp->chunkSize;
        if (chunkSize < bytes) {
        /* Too small; skip it */
            METER(meter.skippedFreeChunks++);
        cpp = &cp->next;
        continue;
        }

        /* We have found a hunk of memory large enough to use */
        p = (PRWord*) cp;
        sp = cp->segment;
        cpNext = cp->next;
#ifndef IS_64
        if (dub && (((PRWord)p & (PR_BYTES_PER_DWORD-1)) == 0)) {
        /*
         * We are double aligning the memory and the current free
         * chunk is aligned on an even boundary. Because header
         * words are one word long we need to discard the first
         * word of memory.
         */
        p[0] = MAKE_HEADER(FREE_MEMORY_TYPEIX, 1);
        SET_HBIT(sp, p);
        p++;
        chunkSize -= PR_BYTES_PER_WORD;
        bytes -= PR_BYTES_PER_WORD;
        PR_ASSERT(((PRWord)p & (PR_BYTES_PER_DWORD-1)) != 0);
        _pr_gcData.freeMemory -= PR_BYTES_PER_WORD;
        _pr_gcData.busyMemory += PR_BYTES_PER_WORD;
        }
#endif
        np = (PRWord*) ((char*) p + bytes);
        remainder = chunkSize - bytes;
        if (remainder >= MIN_FREE_CHUNK_BYTES) {
        /* The left over memory is large enough to be freed. */
        cp = (GCFreeChunk*) np;
        cp->segment = sp;
        cp->chunkSize = remainder;
        InlineBinNumber(newbin, remainder)
        if (newbin != bin) {
            *cpp = (GCFreeChunk*) cpNext; /* remove */
            cp->next = bins[newbin];      /* insert */
            bins[newbin] = cp;
            if (newbin < minBin) minBin = newbin;
            if (newbin > maxBin) maxBin = newbin;
        } else {
            /* Leave it on the same list */
            cp->next = cpNext;
            *cpp = (GCFreeChunk*) np;
        }
        } else {
        /*
         * The left over memory is too small to be released. Just
         * leave it attached to the chunk of memory being
         * returned.
         */
        *cpp = cpNext;
        bytes = chunkSize;
        }
        p[0] = MAKE_HEADER(cbix, (bytes >> PR_BYTES_PER_WORD_LOG2));
        SET_HBIT(sp, p);
        _pr_gcData.freeMemory -= bytes;
        _pr_gcData.busyMemory += bytes;
        return p;
    }
    }
    return 0;
}

/*
** Allocate a piece of memory that is "big" in it's own segment.  Make
** the object consume the entire segment to avoid fragmentation.  When
** the object is no longer referenced, the segment is freed.
*/
static PRWord *BigAlloc(int cbix, PRInt32 bytes, int dub)
{
    GCSeg *sp;
    PRWord *p, h;
    PRInt32 chunkSize;

    /*
    ** If the number of bytes allocated via BigAlloc() since the last GC
    ** exceeds BIG_ALLOC_GC_SIZE then do a GC Now...
    */
    if (bigAllocBytes >= BIG_ALLOC_GC_SIZE) {
        dogc();
    }
    bigAllocBytes += bytes;

    /* Get a segment to hold this allocation */
    sp = GrowHeapExactly(bytes);

    if (sp) {
        p = (PRWord*) sp->base;
        chunkSize = sp->limit - sp->base;

        /* All memory is double aligned on 64 bit machines... */
#ifndef IS_64
        if (dub && (((PRWord)p & (PR_BYTES_PER_DWORD-1)) == 0)) {
            /*
            ** Consume the first word of the chunk with a dummy
            ** unreferenced object.
            */
            p[0] = MAKE_HEADER(FREE_MEMORY_TYPEIX, 1);
            SET_HBIT(sp, p);
            p++;
            chunkSize -= PR_BYTES_PER_WORD;
            _pr_gcData.freeMemory -= PR_BYTES_PER_WORD;
            _pr_gcData.busyMemory += PR_BYTES_PER_WORD;
            PR_ASSERT(((PRWord)p & (PR_BYTES_PER_DWORD-1)) != 0);
        }
#endif

#if defined(WIN16)
            /* All memory MUST be aligned on 32bit boundaries */
            PR_ASSERT( (((PRWord)p) & (PR_BYTES_PER_WORD-1)) == 0 );
#endif

        /* Consume the *entire* segment with a single allocation */
        h = MAKE_HEADER(cbix, (chunkSize >> PR_BYTES_PER_WORD_LOG2));
        p[0] = h;
        SET_HBIT(sp, p);
        _pr_gcData.freeMemory -= chunkSize;
        _pr_gcData.busyMemory += chunkSize;
    return p;
    }
    return 0;
}

/* we disable gc allocation during low memory conditions */
static PRBool allocationEnabled = PR_TRUE;

PR_IMPLEMENT(void) PR_EnableAllocation(PRBool yesOrNo)
{
    allocationEnabled = yesOrNo;
}

static void CollectorCleanup(void) {
    while (collectorCleanupNeeded) {
    LOCK_GC();
    collectorCleanupNeeded = 0;
    UNLOCK_GC();
    if (freeSegs) {
        FreeSegments();
    }
    if (!WEAK_FREELIST_ISEMPTY()) {
        EmptyWeakFreeList();
    }
    }
}

/******************************************************************************/

#ifdef GC_CHECK
static PRInt32 allocationCount;

static void EarthShatteringKaBoom(PRInt32 whichOne) {
    long* p = 0;
    *p = 0;
}

/* Check a segment of heap memory. Verify that the object memory
   hasn't been overwritten (past the end at least) */
static void CheckSegment(GCSeg* sp) {
    PRWord h, tix;
    PRWord *p, *lastp, *np, *limit;

    lastp = p = (PRWord *) sp->base;
    limit = (PRWord *) sp->limit;
    while (p < limit) {
    if (IS_HBIT(sp, p)) {
	    char *cp, i;
	    GCBlockEnd* end;
	    PRWord bytes, requestedBytes;

	    h = p[0];
	    tix = GET_TYPEIX(h);
	    bytes = OBJ_BYTES(h);
	    np = (PRWord *) ((char *)p + bytes);
	    if (tix != FREE_MEMORY_TYPEIX) {
                PRInt32 test;	/* msdev get's fooled without this local */
		/* A live object is here. The last word in the object will
		   contain the objects requestedSize */
		end = (GCBlockEnd*)((char*)(p) + bytes - sizeof(GCBlockEnd));
		test = end->check;
		if (test != PR_BLOCK_END) {
		    PR_ASSERT(test == PR_BLOCK_END);
		}
		requestedBytes = end->requestedBytes;
		if (requestedBytes >= bytes) EarthShatteringKaBoom(0);
		cp = (char*)(p + 1) + requestedBytes;
		i = (char) 0xff;
		while (cp < (char*)end) {
            if (*cp != i) EarthShatteringKaBoom(1);
            cp++;
            i--;
        }
        }
        lastp = p;
        p = np;
    } else {
        /* Must be a freelist item */
        GCFreeChunk *cp = (GCFreeChunk*) p;
        if ((PRInt32)cp->chunkSize < (PRInt32)sizeof(GCFreeChunk)) {
            EarthShatteringKaBoom(3);
        }
        lastp = p;
        p = (PRWord*) ((char*)p + cp->chunkSize);
    }
    }
}

static void CheckHeap(void) {
    GCSeg *sp = segs;
    GCSeg *esp = sp + nsegs;
    while (sp < esp) {
    CheckSegment(sp);
    sp++;
    }
}

#endif /* GC_CHECK */

/******************************************************************************/

#ifdef DEBUG
long gc_thrash = -1L;
#endif

/*
** Allocate memory from the GC Heap. Performs garbage collections if
** memory gets tight and grows the heap as needed. May return NULL if
** memory cannot be found.
*/
PR_IMPLEMENT(PRWord GCPTR *)PR_AllocMemory(
    PRWord requestedBytes, PRInt32 tix, PRWord flags)
{
    PRWord *p;
    CollectorType *ct;
    PRInt32 bytes;
    GCFinal *final = 0;
    GCWeak *weak = 0;
    int dub = flags & PR_ALLOC_DOUBLE;
    PRInt32 objBytes;
#ifdef GC_STATS
    PRInt64 allocTime, ldelta;
#endif

    if (!allocationEnabled) return NULL;

    PR_ASSERT(requestedBytes >= 0);
    PR_ASSERT(_pr_collectorTypes[tix].flags != 0);

#ifdef DEBUG
    if (_pr_do_a_dump) {
    /*
    ** Collect, pause for a second (lets finalizer run), and then GC
    ** again.
    */
    PR_GC();
    PR_Sleep(PR_MicrosecondsToInterval(1000000L));
    PR_GC();
    PR_DumpGCHeap(_pr_dump_file, PR_TRUE);
    _pr_do_a_dump = 0;
    }
#endif

#ifdef GC_STATS
    allocTime = PR_Now();
#endif
    bytes = (PRInt32) requestedBytes;

    /*
    ** Align bytes to a multiple of a PRWord, then add in enough space
    ** to hold the header word.
    **
    ** MSVC 1.52 crashed on the ff. code because of the "complex" shifting :-(
    */
#if !defined(WIN16) 
    /* Check for possible overflow of bytes before performing add */
    if ((MAX_INT - PR_BYTES_PER_WORD) < bytes ) return NULL;
    bytes = (bytes + PR_BYTES_PER_WORD - 1) >> PR_BYTES_PER_WORD_LOG2;
    bytes <<= PR_BYTES_PER_WORD_LOG2;
    /* Check for possible overflow of bytes before performing add */
    if ((MAX_INT - sizeof(PRWord)) < bytes ) return NULL;
    bytes += sizeof(PRWord);
#else 
    /* 
    ** For WIN16 the shifts have been broken out into separate statements
    ** to prevent the compiler from crashing...
    */
    {
        PRWord shiftVal;

        /* Check for possible overflow of bytes before performing add */
        if ((MAX_INT - PR_BYTES_PER_WORD) < bytes ) return NULL;
        bytes += PR_BYTES_PER_WORD - 1L;
        shiftVal = PR_BYTES_PER_WORD_LOG2;
        bytes >>= shiftVal;
        bytes <<= shiftVal;
        /* Check for possible overflow of bytes before performing add */
        if ((MAX_INT - sizeof(PRWord)) < bytes ) return NULL;
        bytes += sizeof(PRWord);
    }
#endif
    /*
     * Add in an extra word of memory for double-aligned memory. Some
     * percentage of the time this will waste a word of memory (too
     * bad). Howver, it makes the allocation logic much simpler and
     * faster.
     */
#ifndef IS_64
    if (dub) {
        /* Check for possible overflow of bytes before performing add */
        if ((MAX_INT - PR_BYTES_PER_WORD) < bytes ) return NULL;
        bytes += PR_BYTES_PER_WORD;
    }
#endif

#ifdef GC_CHECK
    if (_pr_gcData.flags & GC_CHECK) {
        /* Bloat the allocation a bit so that we can lay down
           a check pattern that we will validate */
        /* Check for possible overflow of bytes before performing add */
        if ((MAX_INT - PR_BYTES_PER_WORD * 3) < bytes ) return NULL;
        bytes += PR_BYTES_PER_WORD * 3;
    }
#endif

#if defined(GC_CHECK) || defined(GC_STATS) || defined(GC_TRACEROOTS)
    if ((MAX_INT - sizeof(GCBlockEnd)) < bytes ) return NULL;
    bytes += sizeof(GCBlockEnd);
#endif

    PR_ASSERT( bytes < MAX_ALLOC_SIZE );
    /*
    ** Java can ask for objects bigger than MAX_ALLOC_SIZE,
    ** but it won't get them.
    */
    if (bytes >= MAX_ALLOC_SIZE) return NULL;

#ifdef DEBUG
    if (gc_thrash == -1L ? (gc_thrash = (long)PR_GetEnv("GC_THRASH")):gc_thrash) PR_GC();
#endif

    ct = &_pr_collectorTypes[tix];
    if (ct->flags & (_GC_TYPE_FINAL|_GC_TYPE_WEAK)) {
    if (0 != ct->gctype.finalize) {
        /*
        ** Allocate a GCFinal struct for this object in advance. Don't put
        ** it on the pending list until we have allocated the object
        */
        final = AllocFinalNode();
        if (!final) {
        /* XXX THIS IS NOT ACCEPTABLE*/
		PR_ASSERT(0);
        return 0;
        }
    }
    if (0 != ct->gctype.getWeakLinkOffset) {
        /*
        ** Allocate a GCWeak struct for this object in advance. Don't put
        ** it on the weak links list until we have allocated the object
        */
        weak = AllocWeakNode();
        if (!weak) {
        /* XXX THIS IS NOT ACCEPTABLE*/
        if (0 != final) {
            FreeFinalNode(final);
        }
		PR_ASSERT(0);
        return 0;
        }
    }
    }

    LOCK_GC();
#ifdef GC_CHECK
    if (_pr_gcData.flags & GC_CHECK) CheckHeap();
    allocationCount++;
#endif

    /* Check for overflow of maximum size we can handle */
    if (bytes > MAX_ALLOC) goto lost;

    /* Try default allocation */
    p = ((bytes >= BIG_ALLOC) && (nsegs < MAX_SEGS)) ?
        BigAlloc(tix, bytes, dub) : BinAlloc(tix, bytes, dub);
    if (0 == p) {
#ifdef GC_STATS
        LL_SUB(ldelta, PR_Now(), allocTime);
#endif
        /* Collect some memory */
        _GCTRACE(GC_ALLOC, ("force GC: want %d", bytes));
        dogc();
        PR_ASSERT( GC_IS_LOCKED() );

        /* After a collection we check and see if we should grow the
        ** heap. We grow the heap when the amount of memory free is less
        ** than a certain percentage of the heap size. We don't check to
        ** see if the grow succeeded because our fallback strategy in
        ** either case is to try one more time to allocate. */
        if ((_pr_gcData.allocMemory < _pr_gcData.maxMemory)
        && ((_pr_gcData.freeMemory <
            ((_pr_gcData.allocMemory * MIN_FREE_THRESHOLD_AFTER_GC) / 100L))
        || (_pr_gcData.freeMemory < bytes))) {
            GrowHeap(PR_MAX(bytes, segmentSize));
        }
#ifdef GC_STATS
        LL_ADD(allocTime, PR_Now(), ldelta);
#endif

        /* Try again */
        p = ((bytes >= BIG_ALLOC) && (nsegs < MAX_SEGS)) ?
            BigAlloc(tix, bytes, dub) : BinAlloc(tix, bytes, dub);
        if (0 == p) {
            /* Well that lost big time. Memory must be pretty well fragmented */
            if (!GrowHeap(PR_MAX(bytes, segmentSize))) goto lost;
            p = BinAlloc(tix, bytes, dub);
            if (0 == p) goto lost;
        }
    }

    /* Zero out the portion of the object memory that was used by
       the GCFreeChunk structure (skip the first word because it
       was already overwritten by the gc header word) */
    objBytes = OBJ_BYTES(p[0]);
    if (objBytes > sizeof(PRWord)) p[1] = 0;
    if (objBytes > sizeof(PRWord)*2) p[2] = 0;

    if (final) {
	_GCTRACE(GC_ALLOC, ("alloc 0x%x (%d) final=0x%x",
                p, bytes, final));
    final->object = p;
    PR_APPEND_LINK(&final->links, &_pr_finalizeableObjects);
    } else {
	_GCTRACE(GC_ALLOC, ("alloc 0x%x (%d)", p, bytes));
    }
    if (weak) {
    weak->object = p;
    PR_APPEND_LINK(&weak->links, &_pr_weakLinks);
    }
    METER(meter.allocBytes += bytes);
    METER(meter.wastedBytes += (bytes - requestedBytes));
    UNLOCK_GC();

    if (collectorCleanupNeeded) {
	CollectorCleanup();
    }

#if defined(GC_CHECK) || defined(GC_STATS) || defined(GC_TRACEROOTS)
    {
	GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));
	end->check = PR_BLOCK_END;
    }
#endif
#ifdef GC_STATS
    {
	PRInt64 now = PR_Now();
	double delta;
	PRInt32 bin;
	GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));

	end->allocTime = allocTime;
	LL_SUB(ldelta, now, allocTime);
	LL_L2D(delta, ldelta);
	InlineBinNumber(bin, requestedBytes);
	end->bin = bin;
	gcstats[bin].nallocs++;
	gcstats[bin].allocTime += delta;
	gcstats[bin].allocTimeVariance += delta * delta;
    }
#endif
#ifdef GC_CHECK
    if (_pr_gcData.flags & GC_CHECK) {
    /* Place a pattern in the memory that was allocated that was not
       requested. We will check the pattern later. */
    char* cp = (char*)(p + 1) + requestedBytes;
	GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));
	char i = (char) 0xff;
	while (cp < (char*)end) {
	    *cp++ = i--;
	}
	end->requestedBytes = requestedBytes;
	CheckHeap();
    }
#endif
    return p + 1;

  lost:
    /* Out of memory */
    UNLOCK_GC();
    if (final) {
    FreeFinalNode(final);
    }
    if (weak) {
    FreeWeakNode(weak);
    }
    if (collectorCleanupNeeded) {
    CollectorCleanup();
    }
    return 0;
}

/* Shortcut allocator for objects that do not require finalization or
   are weak objects */
PR_IMPLEMENT(PRWord GCPTR *)
PR_AllocSimpleMemory(PRWord requestedBytes, PRInt32 tix)
{
    PRWord *p;
    PRInt32 bytes;
    PRInt32 objBytes;
#ifdef GC_STATS
    PRInt64 allocTime, ldelta;
#endif

    if (!allocationEnabled) return NULL;

    PR_ASSERT(requestedBytes >= 0);
    PR_ASSERT(_pr_collectorTypes[tix].flags != 0);

#ifdef DEBUG
    if (_pr_do_a_dump) {
	/*
	** Collect, pause for a second (lets finalizer run), and then GC
	** again.
	*/
	PR_GC();
	PR_Sleep(PR_MicrosecondsToInterval(1000000L));
	PR_GC();
	PR_DumpGCHeap(_pr_dump_file, PR_TRUE);
	_pr_do_a_dump = 0;
    }
#endif

#ifdef GC_STATS
    allocTime = PR_NowMS();
#endif
    bytes = (PRInt32) requestedBytes;

    /*
    ** Align bytes to a multiple of a PRWord, then add in enough space
    ** to hold the header word.
    **
    ** MSVC 1.52 crashed on the ff. code because of the "complex" shifting :-(
    */
#if !defined(WIN16) 
    bytes = (bytes + PR_BYTES_PER_WORD - 1) >> PR_BYTES_PER_WORD_LOG2;
    bytes <<= PR_BYTES_PER_WORD_LOG2;
    bytes += sizeof(PRWord);
#else 
    /* 
    ** For WIN16 the shifts have been broken out into separate statements
    ** to prevent the compiler from crashing...
    */
    {
    PRWord shiftVal;

    bytes += PR_BYTES_PER_WORD - 1L;
    shiftVal = PR_BYTES_PER_WORD_LOG2;
    bytes >>= shiftVal;
    bytes <<= shiftVal;
    bytes += sizeof(PRWord);
    }
#endif
    
    /*
     * Add in an extra word of memory for double-aligned memory. Some
     * percentage of the time this will waste a word of memory (too
     * bad). Howver, it makes the allocation logic much simpler and
     * faster.
     */
#ifndef IS_64
    bytes += PR_BYTES_PER_WORD;
#endif

#ifdef GC_CHECK
    if (_pr_gcData.flags & GC_CHECK) {
    /* Bloat the allocation a bit so that we can lay down
       a check pattern that we will validate */
    bytes += PR_BYTES_PER_WORD * 2;
    }
#endif

#if defined(GC_CHECK) || defined(GC_STATS) || defined(GC_TRACEROOTS)
    bytes += sizeof(GCBlockEnd);
#endif

#if defined(WIN16)
    PR_ASSERT( bytes < MAX_ALLOC_SIZE );
#endif
    /* Java can ask for objects bigger than 4M, but it won't get them */
    /*
     * This check was added because there is a fundamental limit of
     * the size field maintained by the gc code.  Going over the 4M
     * limit caused some bits to roll over into another bit field,
     * violating the max segment size and causing a bug.
     */
    if (bytes >= MAX_ALLOC_SIZE) {
        return NULL;
    }
#ifdef DEBUG
    if (gc_thrash == -1L
	? (gc_thrash = (long)PR_GetEnv("GC_THRASH"))
	: gc_thrash) {
	PR_GC();
    }
#endif

    LOCK_GC();
#ifdef GC_CHECK
    if (_pr_gcData.flags & GC_CHECK) {
    CheckHeap();
    }
    allocationCount++;
#endif

    /* Try default allocation */
    if ((bytes >= BIG_ALLOC) && (nsegs < MAX_SEGS)) {
    p = BigAlloc(tix, bytes, 1);
    } else {
    p = BinAlloc(tix, bytes, 1);
    }
    if (0 == p) {
#ifdef GC_STATS
      LL_SUB(ldelta, PR_Now(), allocTime);
#endif
      /* Collect some memory */
      _GCTRACE(GC_ALLOC, ("force GC: want %d", bytes));
      dogc();
      PR_ASSERT( GC_IS_LOCKED() );

      /* After a collection we check and see if we should grow the
     heap. We grow the heap when the amount of memory free is less
     than a certain percentage of the heap size. We don't check to
     see if the grow succeeded because our fallback strategy in
     either case is to try one more time to allocate. */
      if ((_pr_gcData.allocMemory < _pr_gcData.maxMemory) &&
      (_pr_gcData.freeMemory <
       ((_pr_gcData.allocMemory * MIN_FREE_THRESHOLD_AFTER_GC) / 100L))) {
    GrowHeap(PR_MAX(bytes, segmentSize));
      }
#ifdef GC_STATS
      LL_ADD(allocTime, PR_Now(), ldelta);
#endif

      /* Try one last time */
      if ((bytes >= BIG_ALLOC) && (nsegs < MAX_SEGS)) {
    p = BigAlloc(tix, bytes, 1);
      } else {
    p = BinAlloc(tix, bytes, 1);
      }
      if (0 == p) {
    /* Well that lost big time. Memory must be pretty well fragmented */
    if (!GrowHeap(PR_MAX(bytes, segmentSize))) {
      goto lost;
    }
    p = BinAlloc(tix, bytes, 1);
    if (0 == p) goto lost;
      }
    }

    /* Zero out the portion of the object memory that was used by
       the GCFreeChunk structure (skip the first word because it
       was already overwritten by the gc header word) */
    objBytes = OBJ_BYTES(p[0]);
    if (objBytes > sizeof(PRWord)) p[1] = 0;
    if (objBytes > sizeof(PRWord)*2) p[2] = 0;

    METER(meter.allocBytes += bytes);
    METER(meter.wastedBytes += (bytes - requestedBytes));
    UNLOCK_GC();

    if (collectorCleanupNeeded) {
	CollectorCleanup();
    }

#if defined(GC_CHECK) || defined(GC_STATS) || defined(GC_TRACEROOTS)
    {
	GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));
	end->check = PR_BLOCK_END;
    }
#endif
#ifdef GC_STATS
    {
	PRInt64 now = PR_Now();
	double delta;
	PRInt32 bin;
	GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));

	end->allocTime = allocTime;
	LL_SUB(ldelta, now, allocTime);
	LL_L2D(delta, ldelta);
	InlineBinNumber(bin, requestedBytes);
	end->bin = bin;
	gcstats[bin].nallocs++;
	gcstats[bin].allocTime += delta;
	gcstats[bin].allocTimeVariance += delta * delta;
    }
#endif
#ifdef GC_CHECK
    if (_pr_gcData.flags & GC_CHECK) {
    /* Place a pattern in the memory that was allocated that was not
       requested. We will check the pattern later. */
    char* cp = (char*)(p + 1) + requestedBytes;
	GCBlockEnd* end = (GCBlockEnd*)((char*)p + OBJ_BYTES(p[0]) - sizeof(GCBlockEnd));
	char i = (char) 0xff;
	while (cp < (char*)end) {
	    *cp++ = i--;
	}
	end->requestedBytes = requestedBytes;
	CheckHeap();
    }
#endif
    return p + 1;

  lost:
    /* Out of memory */
    UNLOCK_GC();
    if (collectorCleanupNeeded) {
    CollectorCleanup();
    }
    return 0;
}

/************************************************************************/

PR_IMPLEMENT(PRWord) PR_GetObjectHeader(void *ptr) {
    GCSeg *sp;
    PRWord *h;

    if (ptr == 0) return 0;
    sp = InHeap(ptr);
    if (sp == 0) return 0;
    h = (PRWord*)FindObject(sp, (PRWord*)ptr);
    return GC_GET_USER_BITS(h[0]);
}

PR_IMPLEMENT(PRWord) PR_SetObjectHeader(void *ptr, PRWord newUserBits) {
    GCSeg *sp;
    PRWord *h, rv;

    if (ptr == 0) return 0;
    sp = InHeap(ptr);
    if (sp == 0) return 0;
    h = (PRWord*)FindObject(sp, (PRWord*)ptr);
    rv = GC_GET_USER_BITS(h[0]);
    h[0] = (h[0] & ~GC_USER_BITS) |
    ((newUserBits << GC_USER_BITS_SHIFT) & GC_USER_BITS);
    return rv;
}

PR_IMPLEMENT(void) PR_InitGC(
    PRWord flags, PRInt32 initialHeapSize, PRInt32 segSize, PRThreadScope scope)
{
    static char firstTime = 1;

    if (!firstTime) return;
    firstTime = 0;

    _pr_msgc_lm = PR_NewLogModule("msgc");
    _pr_pageShift = PR_GetPageShift();
    _pr_pageSize = PR_GetPageSize();

#if defined(WIN16)
    PR_ASSERT( initialHeapSize < MAX_ALLOC_SIZE );
#endif

  /* Setup initial heap size and initial segment size */
  if (0 != segSize) segmentSize = segSize;
#ifdef DEBUG
    GC = PR_NewLogModule("GC");
    {
    char *ev = PR_GetEnv("GC_SEGMENT_SIZE");
    if (ev && ev[0]) {
      PRInt32 newSegmentSize = atoi(ev);
      if (0 != newSegmentSize) segmentSize = newSegmentSize;
    }
    ev = PR_GetEnv("GC_INITIAL_HEAP_SIZE");
    if (ev && ev[0]) {
      PRInt32 newInitialHeapSize = atoi(ev);
      if (0 != newInitialHeapSize) initialHeapSize = newInitialHeapSize;
    }
    ev = PR_GetEnv("GC_FLAGS");
    if (ev && ev[0]) {
        flags |= atoi(ev);
    }
#ifdef GCMETER
        ev = PR_GetEnv("GC_METER");
        if (ev && ev[0]) {
            _pr_gcMeter = atoi(ev);
        }
#endif
    }
#endif
  if (0 == initialHeapSize) initialHeapSize = segmentSize;
  if (initialHeapSize < segmentSize) initialHeapSize = segmentSize;

  _pr_gcData.maxMemory   = MAX_SEGS * segmentSize;
  _pr_gcData.liveBlock  = ProcessRootBlock;
  _pr_gcData.livePointer = ProcessRootPointer;
  _pr_gcData.processRootBlock  = ProcessRootBlock;
  _pr_gcData.processRootPointer = ProcessRootPointer;
  _pr_gcData.dumpOutput = NULL;

  PR_INIT_CLIST(&_pr_finalizeableObjects);
    PR_INIT_CLIST(&_pr_finalQueue);
    _PR_InitGC(flags);

    /* Create finalizer thread */
    _PR_CreateFinalizer(scope);

  /* Allocate the initial segment for the heap */
  minBin = 31;
  maxBin = 0;
  GrowHeap(initialHeapSize);
    PR_RegisterRootFinder(ScanWeakFreeList, "scan weak free list", 0);
}

#if defined(WIN16)
/*
** For WIN16 the GC_IN_HEAP() macro must call the private InHeap function.
** This public wrapper function makes this possible...
*/
PR_IMPLEMENT(PRBool)
PR_GC_In_Heap(void *object)
{
    return InHeap( object ) != NULL;    
}
#endif


/** Added by Vishy for sanity checking a few GC structures **/
/** Can use SanityCheckGC to debug corrupted GC Heap situations **/

#ifdef DEBUG

static int SegmentOverlaps(int i, int j)
{
  return
    (((segs[i].limit > segs[j].base) && (segs[i].base < segs[j].base)) ||
     ((segs[j].limit > segs[i].base) && (segs[j].base < segs[i].base)));
}

static void NoSegmentOverlaps(void)
{
  int i,j;

  for (i = 0; i < nsegs; i++)
    for (j = i+1 ; j < nsegs ; j++)
      PR_ASSERT(!SegmentOverlaps(i,j));
}

static void SegInfoCheck(void)
{
  int i;
  for (i = 0 ; i < nsegs ; i++)
    PR_ASSERT((segs[i].info->hbits) &&
	      (segs[i].info->hbits == segs[i].hbits) &&
	      (segs[i].info->base == segs[i].base) &&
	      (segs[i].info->limit == segs[i].limit));
}

static void SanityCheckGC()
{
  NoSegmentOverlaps();
  SegInfoCheck();
}

#endif

#if defined(DEBUG) && defined(WIN32)

extern void *baseaddr;
extern void *lastaddr;

PR_IMPLEMENT(void)
PR_PrintGCStats(void)
{
    long reportedSegSpace = _pr_gcData.busyMemory + _pr_gcData.freeMemory;
    char* msg;
    long largeCount = 0, largeSize = 0;
    long segCount = 0, segSize = 0;
    long freeCount = 0, freeSize = 0;
    GCSeg *sp, *esp;
    GCSegInfo* si;

    LOCK_GC();

    sp = segs;
    esp = sp + nsegs;
    while (sp < esp) {
    long size = sp->info->limit - sp->info->base;
    segCount++;
    segSize += size;
        if (sp->info->fromMalloc) {
        largeCount++;
        largeSize += size;
    }
        sp++;
    }

    si = freeSegs;
    while (si != NULL) {
    long size = si->limit - si->base;
    freeCount++;
    freeSize += size;
    si = si->next;
    }
    
    msg = PR_smprintf("\
# GC Stats:\n\
#   vm space:\n\
#     range:      %ld - %ld\n\
#     size:       %ld\n\
#   segments:\n\
#     range:      %ld - %ld\n\
#     count:      %ld (reported: %ld)\n\
#     size:       %ld (reported: %ld)\n\
#     free count: %ld\n\
#     free size:  %ld\n\
#     busy objs:  %ld (%ld%%)\n\
#     free objs:  %ld (%ld%%)\n\
#   large blocks:\n\
#     count:      %ld\n\
#     total size: %ld (%ld%%)\n\
#     avg size:   %ld\n\
",
              /* vm space */
              (long)baseaddr, (long)lastaddr,
              (long)lastaddr - (long)baseaddr,
              /* segments */
              _pr_gcData.lowSeg, _pr_gcData.highSeg,
              segCount, nsegs,
              segSize, reportedSegSpace,
              freeCount,
              freeSize,
              _pr_gcData.busyMemory,
              (_pr_gcData.busyMemory * 100 / reportedSegSpace),
              _pr_gcData.freeMemory,
              (_pr_gcData.freeMemory * 100 / reportedSegSpace),
              /* large blocks */
              largeCount,
              largeSize, (largeSize * 100 / reportedSegSpace),
              (largeCount ? largeSize / largeCount : 0)
              );
    UNLOCK_GC();
    fprintf(stderr, msg);
    OutputDebugString(msg);
    PR_smprintf_free(msg);
#ifdef GC_STATS
    PR_PrintGCAllocStats();
#endif
}
#endif

PR_IMPLEMENT(void)
PR_DumpToFile(char* filename, char* msg, PRFileDumper dump, PRBool detailed)
{
    FILE *out;
    OutputDebugString(msg);
    out = fopen(filename, "a");
    if (!out) {
	char buf[64];
	PR_ASSERT(strlen(filename) < sizeof(buf) - 16);
	PR_snprintf(buf, sizeof(buf), "Can't open \"%s\"\n",
		    filename);
	OutputDebugString(buf);
    }
    else
    {
	struct tm *newtime;
	time_t aclock;
	int i;

	time(&aclock);
	newtime = localtime(&aclock);
	fprintf(out, "%s on %s\n", msg, asctime(newtime));  /* Print current time */
	dump(out, detailed);
	fprintf(out, "\n\n");
	for (i = 0; i < 80; i++)
	    fprintf(out, "=");
	fprintf(out, "\n\n");
	fclose(out);
    }
    OutputDebugString(" done\n");
}

