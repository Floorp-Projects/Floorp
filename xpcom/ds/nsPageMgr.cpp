/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsPageMgr.h"
#include "prmem.h"

#if defined(XP_PC)
#include <windows.h>
#elif defined(XP_MAC)
#include <stdlib.h>
#elif defined(XP_BEOS)
#include <fcntl.h>
#elif defined(XP_UNIX)
#include <sys/mman.h>
#include <fcntl.h>

#ifndef MAP_FAILED
#if defined (__STDC__) && __STDC__
#define MAP_FAILED	((void *) -1)
#else
#define MAP_FAILED	((char *) -1)
#endif
#endif

#if defined(VMS)
#include <starlet.h>
#include <ssdef.h>
#include <vadef.h>
#include <va_rangedef.h>
#endif

#endif

/******************************************************************************/

#define NS_PAGEMGR_CLUSTERDESC_CLUMPSIZE    16

void
nsPageMgr::DeleteFreeClusterDesc(nsClusterDesc *desc)
{
    desc->mNext = mUnusedClusterDescs;
    mUnusedClusterDescs = desc;
}

nsPageMgr::nsClusterDesc*
nsPageMgr::NewFreeClusterDesc(void)
{
    nsClusterDesc *desc = mUnusedClusterDescs;
    if (desc)
        mUnusedClusterDescs = desc->mNext;
    else {
        /* Allocate a clump of cluster records at once, and link all except
           the first onto the list of mUnusedClusterDescs */
        desc = (nsClusterDesc*)PR_Malloc(NS_PAGEMGR_CLUSTERDESC_CLUMPSIZE * sizeof(nsClusterDesc));
        if (desc) {
            nsClusterDesc* desc2 = desc + (NS_PAGEMGR_CLUSTERDESC_CLUMPSIZE - 1);
            while (desc2 != desc) {
                DeleteFreeClusterDesc(desc2--);
            }
        }
    }
    return desc;
}

/* Search the mFreeClusters looking for the first cluster of consecutive free
   pages that is at least size bytes long.  If there is one, remove these pages
   from the free page list and return their address; if not, return nil. */
nsPage*
nsPageMgr::AllocClusterFromFreeList(PRUword nPages)
{
    nsClusterDesc **p = &mFreeClusters;
    nsClusterDesc *desc;
    while ((desc = *p) != NULL) {
        if (desc->mPageCount >= nPages) {
            nsPage* addr = desc->mAddr;
            if (desc->mPageCount == nPages) {
                *p = desc->mNext;
                DeleteFreeClusterDesc(desc);
            }
            else {
                desc->mAddr += nPages;
                desc->mPageCount -= nPages;
            }
            return addr;
        }
        p = &desc->mNext;
    }
    return NULL;
}

/* Add the segment to the nsClusterDesc list, coalescing it with any
   clusters already in the list when possible. */
void
nsPageMgr::AddClusterToFreeList(nsPage* addr, PRWord nPages)
{
    nsClusterDesc **p = &mFreeClusters;
    nsClusterDesc *desc;
    nsClusterDesc *newDesc;

    while ((desc = *p) != NULL) {
        if (desc->mAddr + desc->mPageCount == addr) {
            /* Coalesce with the previous cluster. */
            nsClusterDesc *next = desc->mNext;

            desc->mPageCount += nPages;
            if (next && next->mAddr == addr + nPages) {
                /* We can coalesce with both the previous and the next cluster. */
                desc->mPageCount += next->mPageCount;
                desc->mNext = next->mNext;
                DeleteFreeClusterDesc(next);
            }
            return;
        }
        if (desc->mAddr == addr + nPages) {
            /* Coalesce with the next cluster. */
            desc->mAddr -= nPages;
            desc->mPageCount += nPages;
            return;
        }
        if (desc->mAddr > addr) {
            PR_ASSERT(desc->mAddr > addr + nPages);
            break;
        }
        PR_ASSERT(desc->mAddr + desc->mPageCount < addr);
        p = &desc->mNext;
    }
    newDesc = NewFreeClusterDesc();
    /* In the unlikely event that this malloc fails, we drop the free cluster
       on the floor. The only consequence is that the memory mapping table
       becomes slightly larger. */
    if (newDesc) {
        newDesc->mNext = desc;
        newDesc->mAddr = addr;
        newDesc->mPageCount = nPages;
        *p = newDesc;
    }
}

#ifdef NS_PAGEMGR_VERIFYCLUSTERS

#ifndef XP_PC
#define OutputDebugString(x)    puts(x)
#endif

void
nsPageMgr::VerifyClusters(PRWord nPagesDelta)
{
    static PRUword expectedPagesUsed = 0;
    nsPageCount calculatedPagesUsed;
    nsPage* lastDescEnd = 0;
    nsClusterDesc* desc;
    char str[256];
    expectedPagesUsed += nPagesDelta;
    calculatedPagesUsed = mBoundary - mMemoryBase;
    sprintf(str, "[Clusters: %p", mMemoryBase);
    OutputDebugString(str);
    for (desc = mFreeClusters; desc; desc = desc->mNext) {
        PR_ASSERT(desc->mAddr > lastDescEnd);
        calculatedPagesUsed -= desc->mPageCount;
        lastDescEnd = desc->mAddr + desc->mPageCount;
        sprintf(str, "..%p, %p", desc->mAddr-1, desc->mAddr + desc->mPageCount);
        OutputDebugString(str);
    }
    sprintf(str, "..%p]\n", mBoundary);
    OutputDebugString(str);
    PR_ASSERT(lastDescEnd < mBoundary);
    PR_ASSERT(calculatedPagesUsed == expectedPagesUsed);
}

#endif /* NS_PAGEMGR_VERIFYCLUSTERS */

/*******************************************************************************
 * Machine-dependent stuff
 ******************************************************************************/
 
#if defined(XP_PC)

#define GC_VMBASE               0x40000000      /* XXX move */
#define GC_VMLIMIT              0x0FFFFFFF

#elif defined(XP_MAC)

#define NS_PAGEMGR_MAC_SEGMENT_SIZE     
#define NS_PAGEMGR_MAC_SEGMENT_COUNT    

#endif

PRStatus
nsPageMgr::InitPages(nsPageCount minPages, nsPageCount maxPages)
{
#if defined(XP_PC)

    nsPage* addr = NULL;
    nsPageCount size = maxPages;

#ifdef NS_PAGEMGR_DEBUG
    /* first try to place the heap at a well-known address for debugging */
    addr = (nsPage*)VirtualAlloc((void*)GC_VMBASE, size << NS_PAGEMGR_PAGE_BITS,
                                 MEM_RESERVE, PAGE_READWRITE);
#endif
    while (addr == NULL) {
        /* let the system place the heap */
        addr = (nsPage*)VirtualAlloc(0, size << NS_PAGEMGR_PAGE_BITS,
                                     MEM_RESERVE, PAGE_READWRITE);
        if (addr == NULL) {
            size--;
            if (size < minPages) {
                return PR_FAILURE;
            }
        }
    }
    PR_ASSERT(NS_PAGEMGR_IS_ALIGNED(addr, NS_PAGEMGR_PAGE_BITS));
    mMemoryBase = addr;
    mPageCount = size;
    mBoundary = addr;

    return PR_SUCCESS;

#elif defined(XP_MAC)
    
    OSErr err;
    void* seg;
    void* segLimit;
    Handle h;
    PRUword segSize = (minPages + 1) * NS_PAGEMGR_PAGE_SIZE;
    nsPage* firstPage;
    nsPage* lastPage;
    nsSegmentDesc* mSegTable;
    int mSegTableCount, otherCount;

    h = TempNewHandle(segSize, &err);
    if (err || h == NULL) goto fail;
    MoveHHi(h);
    TempHLock(h, &err);
    if (err) goto fail;
    seg = *h;
    segLimit = (void*)((char*)seg + segSize);
    firstPage = NS_PAGEMGR_PAGE_ROUNDUP(seg);
    lastPage = NS_PAGEMGR_PAGE_ROUNDDN(((char*)seg + segSize));

    /* Put the segment table in the otherwise wasted space at one
       end of the segment. We'll put it at which ever end is bigger. */
    mSegTable = (nsSegmentDesc*)seg;
    mSegTableCount = ((char*)firstPage - (char*)seg) / sizeof(nsSegmentDesc);
    otherCount = ((char*)segLimit - (char*)lastPage) / sizeof(nsSegmentDesc);
    if (otherCount > mSegTableCount) {
        mSegTable = (nsSegmentDesc*)lastPage;
        mSegTableCount = otherCount;
    }
    else if (mSegTableCount == 0) {
        mSegTable = (nsSegmentDesc*)firstPage;
        firstPage++;
        mSegTableCount = NS_PAGEMGR_PAGE_SIZE / sizeof(nsSegmentDesc);
    }
    PR_ASSERT(mSegTableCount > 0);
    mSegTable = mSegTable;
    mSegTableCount = mSegTableCount;
    
    mSegTable[0].mHandle = h;
    mSegTable[0].mFirstPage = firstPage;
    mSegTable[0].mLastPage = lastPage;

    /* XXX hack for now -- just one segment */
    mMemoryBase = firstPage;
    mBoundary = firstPage;
    mPageCount = lastPage - firstPage;

    return PR_SUCCESS;
    
  fail:
    if (h) {
        TempDisposeHandle(h, &err);
    }
    return PR_FAILURE;

#elif defined(XP_BEOS)

    nsPage* addr = NULL;
    nsPageCount size = maxPages;

#if (1L<<NS_PAGEMGR_PAGE_BITS) != B_PAGE_SIZE
#error can only work with 4096 byte pages
#endif
    while(addr == NULL)
	{
        /* let the system place the heap */
		if((mAid = create_area("MozillaHeap", (void **)&addr, B_ANY_ADDRESS,
			size << NS_PAGEMGR_PAGE_BITS, B_NO_LOCK,
			B_READ_AREA | B_WRITE_AREA)) < 0)
		{
			addr = NULL;
            size--;
            if (size < minPages) {
                return PR_FAILURE;
            }
        }
    }
    PR_ASSERT(NS_PAGEMGR_IS_ALIGNED(addr, NS_PAGEMGR_PAGE_BITS));
    mMemoryBase = addr;
    mPageCount = size;
    mBoundary = addr;

    return PR_SUCCESS;

#elif defined(VMS)

    nsPage* addr = NULL;
    nsPageCount size = maxPages;
    struct _va_range retadr, retadr2;
    int status;

    /*
    ** $EXPREG will extend the virtual address region by the requested
    ** number of pages (or pagelets on Alpha). The process must have
    ** sufficient PGFLQUOTA for the operation, otherwise SS$_EXQUOTA will
    ** be returned. However, in the case of SS$_EXQUOTA, $EXPREG will have
    ** grown the region by the largest possible amount. In this case we will
    ** take what we could get, just so long as its over our minimum
    ** threshold.
    */

    status = sys$expreg(size << (NS_PAGEMGR_PAGE_BITS-VA$C_PAGELET_SHIFT_SIZE),
                        &retadr,0,0);
    switch (status) {
        case SS$_NORMAL:
            break;
        case SS$_EXQUOTA:
            size = ( (int)retadr.va_range$ps_end_va -
                     (int)retadr.va_range$ps_start_va + 1
                   ) >> NS_PAGEMGR_PAGE_BITS;
            if (size < minPages) {
                status=sys$deltva(&retadr,&retadr2,0);
                return PR_FAILURE;
            }
            break;
        default:
            return PR_FAILURE;
    }

    /* We got at least something */
    addr = (nsPage *)retadr.va_range$ps_start_va;

    PR_ASSERT(NS_PAGEMGR_IS_ALIGNED(addr, NS_PAGEMGR_PAGE_BITS));
    mMemoryBase = addr;
    mPageCount = size;
    mBoundary = addr;

    return PR_SUCCESS;

#else

    nsPage* addr = NULL;
    nsPageCount size = maxPages;
    mZero_fd = NULL;

    mZero_fd = open("/dev/zero", O_RDWR);

    while (addr == NULL) {
        /* let the system place the heap */
        addr = (nsPage*)mmap(0, size << NS_PAGEMGR_PAGE_BITS,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE,
                             mZero_fd, 0);
        if (addr == (nsPage*)MAP_FAILED) {
            addr = NULL;
            size--;
            if (size < minPages) {
                return PR_FAILURE;
            }
        }
    }
    PR_ASSERT(NS_PAGEMGR_IS_ALIGNED(addr, NS_PAGEMGR_PAGE_BITS));
    mMemoryBase = addr;
    mPageCount = size;
    mBoundary = addr;

    return PR_SUCCESS;
#endif
}

void
nsPageMgr::FinalizePages()
{
#if defined(XP_PC)

    BOOL ok;
    ok = VirtualFree((void*)mMemoryBase, 0, MEM_RELEASE);
    PR_ASSERT(ok);
    mMemoryBase = NULL;
    mPageCount = 0;
    PR_DestroyMonitor(mMonitor);
    mMonitor = NULL;

#elif defined(XP_MAC)

    OSErr err;
    PRUword i;
    for (i = 0; i < mSegTableCount; i++) {
        if (mSegTable[i].mHandle) {
            TempDisposeHandle(mSegTable[i].mHandle, &err);
            PR_ASSERT(err == 0);
        }
    }

#elif defined(XP_BEOS)

	delete_area(mAid);

#elif defined(VMS)

    struct _va_range retadr, retadr2;

    retadr.va_range$ps_start_va = mMemoryBase;
    retadr.va_range$ps_end_va = mMemoryBase +
		(mPageCount << NS_PAGEMGR_PAGE_BITS) - 1;

    sys$deltva(&retadr,&retadr2,0);

#else
    munmap((caddr_t)mMemoryBase, mPageCount << NS_PAGEMGR_PAGE_BITS);
    close(mZero_fd);
#endif
}

/*******************************************************************************
 * Page Manager
 ******************************************************************************/
 
nsPageMgr::nsPageMgr()
    : mUnusedClusterDescs(nsnull),
      mFreeClusters(nsnull),
      mInUseClusters(nsnull),
      mMonitor(nsnull),
      mMemoryBase(nsnull),
      mBoundary(nsnull),
#ifdef XP_PC
      mLastPageFreed(nsnull),
      mLastPageFreedSize(0),
      mLastPageTemp(nsnull),
      mLastPageTempSize(0),
#ifdef NS_PAGEMGR_DEBUG
      mLastPageAllocTries(0),
      mLastPageAllocHits(0),
      mLastPageFreeTries(0),
      mLastPageFreeHits(0),
#endif
#endif
#if defined(XP_MAC)
      mSegMap(nsnull),
      mSegTable(nsnull),
      mSegTableCount(0),
#endif
#if defined(XP_BEOS)
      mAid(B_ERROR),
#endif
      mPageCount(0)
{
    NS_INIT_REFCNT();
}

nsresult
nsPageMgr::Init(nsPageCount minPages, nsPageCount maxPages)
{
    PRStatus status;
    
    mMonitor = PR_NewMonitor();
    if (mMonitor == NULL)
        return PR_FAILURE;

    status = InitPages(minPages, maxPages);
    if (status != PR_SUCCESS)
        return status;

    /* make sure these got set */
    PR_ASSERT(mMemoryBase);
    PR_ASSERT(mBoundary);

    mFreeClusters = NULL;
    mInUseClusters = NULL;
    
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

nsPageMgr::~nsPageMgr()
{
#if defined(XP_PC) && defined(NS_PAGEMGR_DEBUG)
    if (stderr) {
        fprintf(stderr, "Page Manager Cache: alloc hits: %u/%u %u%%, free hits: %u/%u %u%%\n",
                mLastPageAllocHits, mLastPageAllocTries, 
                (mLastPageAllocHits * 100 / mLastPageAllocTries), 
                mLastPageFreeHits, mLastPageFreeTries, 
                (mLastPageFreeHits * 100 / mLastPageFreeTries)); 
    }
#endif
    FinalizePages();

    nsClusterDesc* chain = mUnusedClusterDescs;
    while (chain) {
        nsClusterDesc* desc = chain;
        chain = chain->mNext;
        PR_Free(desc);
    }
}

NS_METHOD
nsPageMgr::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsPageMgr* pageMgr = new nsPageMgr();
    if (pageMgr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(pageMgr);
    nsresult rv = pageMgr->Init();
    if (NS_FAILED(rv)) {
        NS_RELEASE(pageMgr);
        return rv;
    }
    rv = pageMgr->QueryInterface(aIID, aResult);
    NS_RELEASE(pageMgr);
    return NS_OK;
}

NS_IMPL_ADDREF(nsPageMgr);
NS_IMPL_RELEASE(nsPageMgr);

NS_IMETHODIMP
nsPageMgr::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr != nsnull, "null ptr");
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
    if (aIID.Equals(nsIPageManager::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsIPageManager*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIAllocator::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIAllocator*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

/******************************************************************************/

#ifdef XP_PC

#ifdef NS_PAGEMGR_PAGE_HYSTERESIS

#ifdef NS_PAGEMGR_DEBUG

void*
nsPageMgr::NS_PAGEMGR_COMMIT_CLUSTER(void* addr, PRUword size) 
{
    mLastPageAllocTries++;
    if (mLastPageFreed == (void*)(addr) && mLastPageFreedSize == (size)) {
#ifdef NS_PAGEMGR_COMMIT_TRACE
        char buf[64];
        PR_snprintf(buf, sizeof(buf), "lalloc %p %u\n",
                    mLastPageFreed, mLastPageFreedSize);
        OutputDebugString(buf);
#endif
        DBG_MEMSET(mLastPageFreed, NS_PAGEMGR_PAGE_ALLOC_PATTERN, mLastPageFreedSize);
        mLastPageTemp = mLastPageFreed;
        mLastPageFreed = NULL;
        mLastPageFreedSize = 0;
        mLastPageAllocHits++;
        return mLastPageTemp; 
    } 
    else {
        /* If the cached pages intersect the current request, we lose. 
           Just free the cached request instead of trying to split it up. */
        if (mLastPageFreed &&
            nsOverlapping((char*)mLastPageFreed, ((char*)mLastPageFreed + mLastPageFreedSize),
                          (char*)(addr), ((char*)(addr) + (size)))
//            ((char*)mLastPageFreed < ((char*)(addr) + (size))
//             && ((char*)mLastPageFreed + mLastPageFreedSize) > (char*)(addr))
            ) {
#ifdef NS_PAGEMGR_COMMIT_TRACE
            char buf[64];
            PR_snprintf(buf, sizeof(buf), "valloc %p %u (vfree  %p %u last=%u:%u req=%u:%u)\n",
                        addr, size,
                        mLastPageFreed, mLastPageFreedSize,
                        (char*)mLastPageFreed, ((char*)mLastPageFreed + mLastPageFreedSize),
                        (char*)(addr), ((char*)(addr) + (size)));
            OutputDebugString(buf);
#endif
            VirtualFree(mLastPageFreed, mLastPageFreedSize, MEM_DECOMMIT);
            mLastPageFreed = NULL;
            mLastPageFreedSize = 0;
            mLastPageFreeHits--;      /* lost after all */
        }
        else {
#ifdef NS_PAGEMGR_COMMIT_TRACE
            char buf[64];
            PR_snprintf(buf, sizeof(buf), "valloc %p %u (skipping %p %u)\n",
                        addr, size,
                        mLastPageFreed, mLastPageFreedSize);
            OutputDebugString(buf);
#endif
        }
        return VirtualAlloc((void*)(addr), (size), MEM_COMMIT, PAGE_READWRITE); 
    } 
} 

int
nsPageMgr::NS_PAGEMGR_DECOMMIT_CLUSTER(void* addr, PRUword size) 
{
    mLastPageFreeTries++;
    PR_ASSERT(mLastPageFreed != (void*)(addr));
    if (mLastPageFreed) {
        /* If we've already got a cached page, just keep it. Heuristically,
           this tends to give us a higher hit rate because of the order in
           which pages are decommitted. */
#ifdef NS_PAGEMGR_COMMIT_TRACE
        char buf[64];
        PR_snprintf(buf, sizeof(buf), "vfree  %p %u (cached %p %u)\n", 
                    addr, size, mLastPageFreed, mLastPageFreedSize);
        OutputDebugString(buf);
#endif
        return VirtualFree(addr, size, MEM_DECOMMIT); 
    }
    mLastPageFreed = (void*)(addr); 
    mLastPageFreedSize = (size); 
    DBG_MEMSET(mLastPageFreed, NS_PAGEMGR_PAGE_FREE_PATTERN, mLastPageFreedSize); 
#ifdef NS_PAGEMGR_COMMIT_TRACE
    {
        char buf[64];
        PR_snprintf(buf, sizeof(buf), "lfree  %p %u\n",
                    mLastPageFreed, mLastPageFreedSize);
        OutputDebugString(buf);
    }
#endif
    mLastPageFreeHits++;
    return 1; 
} 

#else /* !NS_PAGEMGR_DEBUG */

#define NS_PAGEMGR_COMMIT_CLUSTER(addr, size)                                           \
    (PR_ASSERT((void*)(addr) != NULL),                                                  \
     ((mLastPageFreed == (void*)(addr) && mLastPageFreedSize == (size))                 \
      ? (DBG_MEMSET(mLastPageFreed, NS_PAGEMGR_PAGE_ALLOC_PATTERN, mLastPageFreedSize), \
         mLastPageTemp = mLastPageFreed,                                                \
         mLastPageFreed = NULL,                                                         \
         mLastPageFreedSize = 0,                                                        \
         mLastPageTemp)                                                                 \
      : (((mLastPageFreed &&                                                            \
           ((char*)mLastPageFreed < ((char*)(addr) + (size))                            \
            && ((char*)mLastPageFreed + mLastPageFreedSize) > (char*)(addr)))           \
          ? (VirtualFree(mLastPageFreed, mLastPageFreedSize, MEM_DECOMMIT),             \
             mLastPageFreed = NULL,                                                     \
             mLastPageFreedSize = 0)                                                    \
          : ((void)0)),                                                                 \
         VirtualAlloc((void*)(addr), (size), MEM_COMMIT, PAGE_READWRITE))))             \

#define NS_PAGEMGR_DECOMMIT_CLUSTER(addr, size)                                         \
    (PR_ASSERT(mLastPageFreed != (void*)(addr)),                                        \
     (mLastPageFreed                                                                    \
      ? (VirtualFree(addr, size, MEM_DECOMMIT))                                         \
      : (mLastPageFreed = (addr),                                                       \
         mLastPageFreedSize = (size),                                                   \
         DBG_MEMSET(mLastPageFreed, NS_PAGEMGR_PAGE_FREE_PATTERN, mLastPageFreedSize),  \
         1)))                                                                           \

#endif /* !NS_PAGEMGR_DEBUG */

#else /* !NS_PAGEMGR_PAGE_HYSTERESIS */

#define NS_PAGEMGR_COMMIT_CLUSTER(addr, size) \
    VirtualAlloc((void*)(addr), (size), MEM_COMMIT, PAGE_READWRITE)

#define NS_PAGEMGR_DECOMMIT_CLUSTER(addr, size) \
    VirtualFree((void*)(addr), (size), MEM_DECOMMIT)

#endif /* !NS_PAGEMGR_PAGE_HYSTERESIS */

#else /* !XP_PC */

#define NS_PAGEMGR_COMMIT_CLUSTER(addr, size)      (addr)
#define NS_PAGEMGR_DECOMMIT_CLUSTER(addr, size)    1

#endif /* !XP_PC */

nsPage*
nsPageMgr::NewCluster(nsPageCount nPages)
{
    nsAutoMonitor mon(mMonitor);

    nsPage* addr;
    PR_ASSERT(nPages > 0);
    addr = AllocClusterFromFreeList(nPages);
    if (!addr && mBoundary + nPages <= mMemoryBase + mPageCount) {
        addr = mBoundary;
        mBoundary += nPages;
    }
    if (addr) {
        /* Extend the mapping */
        nsPage* vaddr;
        PRUword size = nPages << NS_PAGEMGR_PAGE_BITS;
        PR_ASSERT(NS_PAGEMGR_IS_ALIGNED(addr, NS_PAGEMGR_PAGE_BITS));
        vaddr = (nsPage*)NS_PAGEMGR_COMMIT_CLUSTER((void*)addr, size);
#ifdef NS_PAGEMGR_VERIFYCLUSTERS
        VerifyClusters(nPages);
#endif
        if (addr) {
            PR_ASSERT(vaddr == addr);
        }
        else {
            DestroyCluster(addr, nPages);
        }
        DBG_MEMSET(addr, NS_PAGEMGR_PAGE_ALLOC_PATTERN, size);
    }
    return (nsPage*)addr;
}

void
nsPageMgr::DestroyCluster(nsPage* basePage, nsPageCount nPages)
{
    nsAutoMonitor mon(mMonitor);

    int freeResult;
    PRUword size = nPages << NS_PAGEMGR_PAGE_BITS;

    PR_ASSERT(nPages > 0);
    PR_ASSERT(NS_PAGEMGR_IS_ALIGNED(basePage, NS_PAGEMGR_PAGE_BITS));
    PR_ASSERT(mMemoryBase <= basePage);
    PR_ASSERT(basePage + nPages <= mMemoryBase + mPageCount);
    DBG_MEMSET(basePage, NS_PAGEMGR_PAGE_FREE_PATTERN, size);
    freeResult = NS_PAGEMGR_DECOMMIT_CLUSTER((void*)basePage, size);
    PR_ASSERT(freeResult);
    if (basePage + nPages == mBoundary) {
        nsClusterDesc **p;
        nsClusterDesc *desc;
        /* We deallocated the last set of clusters. Move the mBoundary lower. */
        mBoundary = basePage;
        /* The last free cluster might now be adjacent to the mBoundary; if so,
           move the mBoundary before that cluster and delete that cluster
           altogether. */
        p = &mFreeClusters;
        while ((desc = *p) != NULL) {
            if (!desc->mNext && desc->mAddr + desc->mPageCount == mBoundary) {
                *p = 0;
                mBoundary = desc->mAddr;
                DeleteFreeClusterDesc(desc);
            }
            else {
                p = &desc->mNext;
            }
        }
    }
    else {
        AddClusterToFreeList(basePage, nPages);
    }
#ifdef NS_PAGEMGR_VERIFYCLUSTERS
    VerifyClusters(-(PRWord)nPages);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// nsIPageManager methods:

NS_IMETHODIMP
nsPageMgr::AllocPages(PRUint32 pageCount, void* *result)
{
    nsPage* page = NewCluster(NS_STATIC_CAST(nsPageCount, pageCount));
    if (page == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = page;
    return NS_OK;
}

NS_IMETHODIMP
nsPageMgr::DeallocPages(PRUint32 pageCount, void* pages)
{
    DestroyCluster(NS_STATIC_CAST(nsPage*, pages),
                   NS_STATIC_CAST(nsPageCount, pageCount));
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAllocator methods:
//
// Note: nsIAllocator needs to keep track of the size of the blocks it allocates
// whereas, nsIPageManager doesn't. That means that there's a little extra 
// overhead for users of this interface. It also means that things allocated
// with the nsIPageManager interface can't be freed with the nsIAllocator 
// interface and vice versa.

NS_IMETHODIMP_(void*)
nsPageMgr::Alloc(PRUint32 size)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;
    void* page = nsnull;
    PRUint32 pageCount = NS_PAGEMGR_PAGE_COUNT(size);

    rv = AllocPages(pageCount, &page);
    if (NS_FAILED(rv)) 
        return nsnull;

    // Add this cluster to the mInUseClusters list:
    nsClusterDesc* desc = NewFreeClusterDesc();
    if (desc == nsnull) {
        rv = DeallocPages(pageCount, page);
        NS_ASSERTION(NS_SUCCEEDED(rv), "DeallocPages failed");
        return nsnull;
    }
    desc->mAddr = (nsPage*)page;
    desc->mPageCount = pageCount;
    desc->mNext = mInUseClusters;
    mInUseClusters = desc;

    return page;
}

NS_IMETHODIMP_(void*)
nsPageMgr::Realloc(void* ptr, PRUint32 size)
{
    // XXX This realloc implementation could be made smarter by trying to 
    // append to the current block, but I don't think we really care right now.
    nsresult rv;
    rv = Free(ptr);
    if (NS_FAILED(rv)) return nsnull;
    void* newPtr = Alloc(size);
    return newPtr;
}

NS_IMETHODIMP
nsPageMgr::Free(void* ptr)
{
    nsAutoMonitor mon(mMonitor);

    PR_ASSERT(NS_PAGEMGR_IS_ALIGNED(ptr, NS_PAGEMGR_PAGE_BITS));

    // Remove the cluster from the mInUseClusters list:
    nsClusterDesc** list = &mInUseClusters;
    nsClusterDesc* desc;
    while ((desc = *list) != nsnull) {
        if (desc->mAddr == ptr) {
            // found -- unlink the desc and free it
            *list = desc->mNext;
            nsresult rv = DeallocPages(desc->mPageCount, ptr);
            DeleteFreeClusterDesc(desc);
            return rv;
        }
		list = &desc->mNext;
    }
    NS_ASSERTION(0, "memory not allocated with nsPageMgr::Alloc");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPageMgr::HeapMinimize(void)
{
    // can't compact this heap
    return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

