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

#ifndef nsPageMgr_h__
#define nsPageMgr_h__

#include "nsIPageManager.h"
#include "nsIAllocator.h"

#include "nscore.h"
#include "nsAutoLock.h"
#include "prlog.h"

#ifdef XP_MAC
#include <Types.h>
#include <Memory.h>
#endif

#if defined(XP_BEOS)
#include <OS.h>
#endif

/*******************************************************************************
 * Configuration/Debugging parameters:
 ******************************************************************************/

#ifdef NS_DEBUG
//#define NS_PAGEMGR_DEBUG
#endif

#define NS_PAGEMGR_PAGE_HYSTERESIS      /* undef if you want to compare */
//#define NS_PAGEMGR_COMMIT_TRACE

#ifdef NS_PAGEMGR_DEBUG
#define NS_PAGEMGR_VERIFYCLUSTERS
#define NS_PAGEMGR_DBG_MEMSET
#endif

#define NS_PAGEMGR_MIN_PAGES            32      // XXX bogus -- this should be a runtime parameter
#define NS_PAGEMGR_MAX_PAGES            2560    // 10 meg    XXX bogus -- this should be a runtime parameter

/******************************************************************************/

#ifdef NS_PAGEMGR_DBG_MEMSET

#include <string.h> /* for memset */
#include <stdio.h>

#define DBG_MEMSET(dest, pattern, size) memset(dest, pattern, size)

#define NS_PAGEMGR_PAGE_ALLOC_PATTERN   0xCB
#define NS_PAGEMGR_PAGE_FREE_PATTERN    0xCD

#else

#define DBG_MEMSET(dest, pattern, size) ((void)0)

#endif

#define NS_PAGEMGR_ALIGN(p, nBits)      ((PRWord)(p) & ~((1 << nBits) - 1))
#define NS_PAGEMGR_IS_ALIGNED(p, nBits) ((PRWord)(p) == NS_PAGEMGR_ALIGN(p, nBits))

/*******************************************************************************
 * Test for overlapping (one-dimensional) regions
 ******************************************************************************/

inline PRBool nsOverlapping(char* min1, char* max1, char* min2, char* max2)
{
    PR_ASSERT(min1 < max1);
    PR_ASSERT(min2 < max2);
    return (min1 < max2 && max1 > min2);
}

/*******************************************************************************
 * Types
 ******************************************************************************/

typedef PRUint8 nsPage[NS_PAGEMGR_PAGE_SIZE];
typedef PRUword nsPageCount;    /* int big enough to count pages */

/*******************************************************************************
 * Macros
 ******************************************************************************/

#define NS_PAGEMGR_PAGE_ROUNDDN(addr)   ((nsPage*)NS_PAGEMGR_ALIGN((PRUword)(addr), NS_PAGEMGR_PAGE_BITS))
#define NS_PAGEMGR_PAGE_ROUNDUP(addr)   NS_PAGEMGR_PAGE_ROUNDDN((PRUword)(addr) + NS_PAGEMGR_PAGE_SIZE)

/*******************************************************************************
 * Page Manager
 ******************************************************************************/

class nsPageMgr : public nsIPageManager, public nsIAllocator {
  public:
    NS_DECL_ISUPPORTS

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    // nsIPageManager methods:
    NS_IMETHOD AllocPages(PRUint32 pageCount, void* *result);
    NS_IMETHOD DeallocPages(PRUint32 pageCount, void* pages);

    // nsIAllocator methods:
    NS_IMETHOD_(void*) Alloc(PRUint32 size);
    NS_IMETHOD_(void*) Realloc(void* ptr, PRUint32 size);
    NS_IMETHOD Free(void* ptr);
    NS_IMETHOD HeapMinimize(void);

    // nsPageMgr methods:
    nsPageMgr();
    virtual ~nsPageMgr();

    nsresult Init(nsPageCount minPages = NS_PAGEMGR_MIN_PAGES,
                  nsPageCount maxPages = NS_PAGEMGR_MAX_PAGES);

    struct nsClusterDesc {
        nsClusterDesc*  mNext;   /* Link to next cluster of free pages */
        nsPage*         mAddr;   /* First page in cluster of free pages */
        nsPageCount     mPageCount; /* Total size of cluster of free pages in bytes */
    };

    void DeleteFreeClusterDesc(nsClusterDesc *desc);
    nsClusterDesc* NewFreeClusterDesc(void);
    nsPage* AllocClusterFromFreeList(PRUword nPages);
    void AddClusterToFreeList(nsPage* addr, PRWord nPages);
#ifdef NS_PAGEMGR_VERIFYCLUSTERS
    void VerifyClusters(PRWord nPagesDelta);
#endif

#if defined(XP_PC) && defined(NS_PAGEMGR_DEBUG)
    void* NS_PAGEMGR_COMMIT_CLUSTER(void* addr, PRUword size);
    int NS_PAGEMGR_DECOMMIT_CLUSTER(void* addr, PRUword size);
#endif

    // Managing Pages:
    PRStatus InitPages(nsPageCount minPages, nsPageCount maxPages);
    void FinalizePages();

    nsPage* NewCluster(nsPageCount nPages);
    void DestroyCluster(nsPage* basePage, nsPageCount nPages);

  protected:
    nsClusterDesc*      mUnusedClusterDescs;
    nsClusterDesc*      mFreeClusters;
    nsClusterDesc*      mInUseClusters; // used by nsIAllocator methods
    PRMonitor*          mMonitor;
    nsPage*             mMemoryBase;
    nsPage*             mBoundary;
    nsPageCount         mPageCount;

#ifdef XP_PC
    /* one-page hysteresis */
    void*               mLastPageFreed;
    PRUword             mLastPageFreedSize;
    void*               mLastPageTemp;
    PRUword             mLastPageTempSize;
# ifdef NS_PAGEMGR_DEBUG
    PRUword             mLastPageAllocTries;
    PRUword             mLastPageAllocHits;
    PRUword             mLastPageFreeTries;
    PRUword             mLastPageFreeHits;
# endif
#endif

#if defined(XP_MAC)
    struct nsSegmentDesc {
        Handle          mHandle;
        nsPage*         mFirstPage;
        nsPage*         mLastPage;
    };
    PRUint8*            mSegMap;
    nsSegmentDesc*      mSegTable;
    PRWord              mSegTableCount;
#endif

#if defined(XP_BEOS)
	area_id				mAid;
#endif

#if (! defined(VMS)) && (! defined(XP_BEOS)) && (! defined(XP_MAC)) && (! defined(XP_PC))
    int mZero_fd;
#endif

};

/******************************************************************************/

#endif /* nsPageMgr_h__ */
