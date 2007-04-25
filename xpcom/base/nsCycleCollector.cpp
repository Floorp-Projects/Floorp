/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set cindent tabstop=4 expandtab shiftwidth=4: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

//
// This file implements a garbage-cycle collector based on the paper
// 
//   Concurrent Cycle Collection in Reference Counted Systems
//   Bacon & Rajan (2001), ECOOP 2001 / Springer LNCS vol 2072
//
// We are not using the concurrent or acyclic cases of that paper; so
// the green, red and orange colors are not used.
//
// The collector is based on tracking pointers of four colors:
//
// Black nodes are definitely live. If we ever determine a node is
// black, it's ok to forget about, drop from our records.
//
// White nodes are definitely garbage cycles. Once we finish with our
// scanning, we unlink all the white nodes and expect that by
// unlinking them they will self-destruct (since a garbage cycle is
// only keeping itself alive with internal links, by definition).
//
// Grey nodes are being scanned. Nodes that turn grey will turn
// either black if we determine that they're live, or white if we
// determine that they're a garbage cycle. After the main collection
// algorithm there should be no grey nodes.
//
// Purple nodes are *candidates* for being scanned. They are nodes we
// haven't begun scanning yet because they're not old enough, or we're
// still partway through the algorithm.
//
// XPCOM objects participating in garbage-cycle collection are obliged
// to inform us when they ought to turn purple; that is, when their
// refcount transitions from N+1 -> N, for nonzero N. Furthermore we
// require that *after* an XPCOM object has informed us of turning
// purple, they will tell us when they either transition back to being
// black (incremented refcount) or are ultimately deleted.


// Safety:
//
// An XPCOM object is either scan-safe or scan-unsafe, purple-safe or
// purple-unsafe.
//
// An object is scan-safe if:
//
//  - It can be QI'ed to |nsCycleCollectionParticipant|, though this
//    operation loses ISupports identity (like nsIClassInfo).
//  - The operation |traverse| on the resulting
//    nsCycleCollectionParticipant does not cause *any* refcount
//    adjustment to occur (no AddRef / Release calls).
//
// An object is purple-safe if it satisfies the following properties:
//
//  - The object is scan-safe.  
//  - If the object calls |nsCycleCollector::suspect(this)|, 
//    it will eventually call |nsCycleCollector::forget(this)|, 
//    exactly once per call to |suspect|, before being destroyed.
//
// When we receive a pointer |ptr| via
// |nsCycleCollector::suspect(ptr)|, we assume it is purple-safe. We
// can check the scan-safety, but have no way to ensure the
// purple-safety; objects must obey, or else the entire system falls
// apart. Don't involve an object in this scheme if you can't
// guarantee its purple-safety.
//
// When we have a scannable set of purple nodes ready, we begin
// our walks. During the walks, the nodes we |traverse| should only
// feed us more scan-safe nodes, and should not adjust the refcounts
// of those nodes. 
//
// We do not |AddRef| or |Release| any objects during scanning. We
// rely on purple-safety of the roots that call |suspect| and
// |forget| to hold, such that we will forget about a purple pointer
// before it is destroyed.  The pointers that are merely scan-safe,
// we hold only for the duration of scanning, and there should be no
// objects released from the scan-safe set during the scan (there
// should be no threads involved).
//
// We *do* call |AddRef| and |Release| on every white object, on
// either side of the calls to |Unlink|. This keeps the set of white
// objects alive during the unlinking.
// 

#ifndef __MINGW32__
#ifdef WIN32
#include <crtdbg.h>
#include <errno.h>
#endif
#endif

#include "nsCycleCollectionParticipant.h"
#include "nsIProgrammingLanguage.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsDeque.h"
#include "nsCycleCollector.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prprf.h"
#include "plstr.h"
#include "prtime.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"

#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <process.h>
#endif

#define DEFAULT_SHUTDOWN_COLLECTIONS 5
#ifdef DEBUG_CC
#define SHUTDOWN_COLLECTIONS(params) params.mShutdownCollections
#else
#define SHUTDOWN_COLLECTIONS(params) DEFAULT_SHUTDOWN_COLLECTIONS
#endif

// Various parameters of this collector can be tuned using environment
// variables.

struct nsCycleCollectorParams
{
    PRBool mDoNothing;
#ifdef DEBUG_CC
    PRBool mReportStats;
    PRBool mHookMalloc;
    PRBool mDrawGraphs;
    PRBool mFaultIsFatal;
    PRBool mLogPointers;

    PRUint32 mShutdownCollections;
#endif
    
    PRUint32 mScanDelay;
    
    nsCycleCollectorParams() :
#ifdef DEBUG_CC
        mDoNothing     (PR_GetEnv("XPCOM_CC_DO_NOTHING") != NULL),
        mReportStats   (PR_GetEnv("XPCOM_CC_REPORT_STATS") != NULL),
        mHookMalloc    (PR_GetEnv("XPCOM_CC_HOOK_MALLOC") != NULL),
        mDrawGraphs    (PR_GetEnv("XPCOM_CC_DRAW_GRAPHS") != NULL),
        mFaultIsFatal  (PR_GetEnv("XPCOM_CC_FAULT_IS_FATAL") != NULL),
        mLogPointers   (PR_GetEnv("XPCOM_CC_LOG_POINTERS") != NULL),

        mShutdownCollections(DEFAULT_SHUTDOWN_COLLECTIONS),
#else
        mDoNothing     (PR_FALSE),
#endif

        // The default number of collections to "age" candidate
        // pointers in the purple buffer before we decide that any
        // garbage cycle they're in has stabilized and we want to
        // consider scanning it.
        //
        // Making this number smaller causes:
        //   - More time to be spent in the collector (bad)
        //   - Less delay between forming garbage and collecting it (good)

        mScanDelay(10)
    {
#ifdef DEBUG_CC
        char *s = PR_GetEnv("XPCOM_CC_SCAN_DELAY");
        if (s)
            PR_sscanf(s, "%d", &mScanDelay);
        s = PR_GetEnv("XPCOM_CC_SHUTDOWN_COLLECTIONS");
        if (s)
            PR_sscanf(s, "%d", &mShutdownCollections);
#endif
    }
};

#ifdef DEBUG_CC
// Various operations involving the collector are recorded in a
// statistics table. These are for diagnostics.

struct nsCycleCollectorStats
{
    PRUint32 mFailedQI;
    PRUint32 mSuccessfulQI;

    PRUint32 mVisitedNode;
    PRUint32 mVisitedJSNode;
    PRUint32 mWalkedGraph;
    PRUint32 mCollectedBytes;
    PRUint32 mFreeCalls;
    PRUint32 mFreedBytes;

    PRUint32 mSetColorGrey;
    PRUint32 mSetColorBlack;
    PRUint32 mSetColorWhite;

    PRUint32 mFailedUnlink;
    PRUint32 mCollectedNode;
    PRUint32 mBumpGeneration;
    PRUint32 mZeroGeneration;

    PRUint32 mSuspectNode;
    PRUint32 mSpills;    
    PRUint32 mForgetNode;
    PRUint32 mFreedWhilePurple;
  
    PRUint32 mCollection;

    nsCycleCollectorStats()
    {
        memset(this, 0, sizeof(nsCycleCollectorStats));
    }
  
    void Dump()
    {
        fprintf(stderr, "\f\n");
#define DUMP(entry) fprintf(stderr, "%30.30s: %-20.20d\n", #entry, entry)
        DUMP(mFailedQI);
        DUMP(mSuccessfulQI);
    
        DUMP(mVisitedNode);
        DUMP(mVisitedJSNode);
        DUMP(mWalkedGraph);
        DUMP(mCollectedBytes);
        DUMP(mFreeCalls);
        DUMP(mFreedBytes);
    
        DUMP(mSetColorGrey);
        DUMP(mSetColorBlack);
        DUMP(mSetColorWhite);
    
        DUMP(mFailedUnlink);
        DUMP(mCollectedNode);
        DUMP(mBumpGeneration);
        DUMP(mZeroGeneration);
    
        DUMP(mSuspectNode);
        DUMP(mSpills);
        DUMP(mForgetNode);
        DUMP(mFreedWhilePurple);
    
        DUMP(mCollection);
#undef DUMP
    }
};
#endif

static void
ToParticipant(nsISupports *s, nsCycleCollectionParticipant **cp);

#ifdef DEBUG_CC
static PRBool
nsCycleCollector_shouldSuppress(nsISupports *s);
#endif

////////////////////////////////////////////////////////////////////////
// Base types
////////////////////////////////////////////////////////////////////////

struct PtrInfo;

class EdgePool
{
public:
    // EdgePool allocates arrays of void*, primarily to hold PtrInfo*.
    // However, at the end of a block, the last two pointers are a null
    // and then a void** pointing to the next block.  This allows
    // EdgePool::Iterators to be a single word but still capable of crossing
    // block boundaries.

    EdgePool()
    {
        mSentinelAndBlocks[0].block = nsnull;
        mSentinelAndBlocks[1].block = nsnull;
    }

    ~EdgePool()
    {
        Block *b = Blocks();
        while (b) {
            Block *next = b->Next();
            delete b;
            b = next;
        }
    }

private:
    struct Block;
    union PtrInfoOrBlock {
        // Use a union to avoid reinterpret_cast and the ensuing
        // potential aliasing bugs.
        PtrInfo *ptrInfo;
        Block *block;
    };
    struct Block {
        enum { BlockSize = 64 * 1024 };

        PtrInfoOrBlock mPointers[BlockSize];
        Block() {
            mPointers[BlockSize - 2].block = nsnull; // sentinel
            mPointers[BlockSize - 1].block = nsnull; // next block pointer
        }
        Block*& Next()
            { return mPointers[BlockSize - 1].block; }
        PtrInfoOrBlock* Start()
            { return &mPointers[0]; }
        PtrInfoOrBlock* End()
            { return &mPointers[BlockSize - 2]; }
    };

    // Store the null sentinel so that we can have valid iterators
    // before adding any edges and without adding any blocks.
    PtrInfoOrBlock mSentinelAndBlocks[2];

    Block*& Blocks() { return mSentinelAndBlocks[1].block; }

public:
    class Iterator
    {
    public:
        Iterator() : mPointer(nsnull) {}
        Iterator(PtrInfoOrBlock *aPointer) : mPointer(aPointer) {}
        Iterator(const Iterator& aOther) : mPointer(aOther.mPointer) {}

        Iterator& operator++()
        {
            if (mPointer->ptrInfo == nsnull) {
                // Null pointer is a sentinel for link to the next block.
                mPointer = (mPointer + 1)->block->mPointers;
            }
            ++mPointer;
            return *this;
        }

        PtrInfo* operator*() const
        {
            if (mPointer->ptrInfo == nsnull) {
                // Null pointer is a sentinel for link to the next block.
                return (mPointer + 1)->block->mPointers->ptrInfo;
            }
            return mPointer->ptrInfo;
        }
        PRBool operator==(const Iterator& aOther) const
            { return mPointer == aOther.mPointer; }
        PRBool operator!=(const Iterator& aOther) const
            { return mPointer != aOther.mPointer; }

    private:
        PtrInfoOrBlock *mPointer;
    };

    class Builder;
    friend class Builder;
    class Builder {
    public:
        Builder(EdgePool &aPool)
            : mCurrent(&aPool.mSentinelAndBlocks[0]),
              mBlockEnd(&aPool.mSentinelAndBlocks[0]),
              mNextBlockPtr(&aPool.Blocks())
        {
        }

        EdgePool::Iterator Mark() { return EdgePool::Iterator(mCurrent); }

        void Add(PtrInfo* aEdge) {
            if (mCurrent == mBlockEnd) {
                EdgePool::Block *b = new EdgePool::Block();
                if (!b) {
                    // This means we just won't collect (some) cycles.
                    NS_NOTREACHED("out of memory, ignoring edges");
                    return;
                }
                *mNextBlockPtr = b;
                mCurrent = b->Start();
                mBlockEnd = b->End();
                mNextBlockPtr = &b->Next();
            }
            (mCurrent++)->ptrInfo = aEdge;
        }
    private:
        // mBlockEnd points to space for null sentinel
        PtrInfoOrBlock *mCurrent, *mBlockEnd;
        EdgePool::Block **mNextBlockPtr;
    };

};


enum NodeColor { black, white, grey };

// This structure should be kept as small as possible; we may expect
// a million of them to be allocated and touched repeatedly during
// each cycle collection.

struct PtrInfo
{
    void *mPointer;
    PRUint32 mColor : 2;
    PRUint32 mInternalRefs : 30;
    // FIXME: mLang expands back to a full word when bug 368774 lands.
    PRUint32 mLang : 2;
    PRUint32 mRefCount : 30;
    EdgePool::Iterator mFirstChild; // first
    EdgePool::Iterator mLastChild; // one after last

#ifdef DEBUG_CC
    size_t mBytes;
    const char *mName;
#endif

    PtrInfo(void *aPointer)
        : mPointer(aPointer),
          mColor(grey),
          mInternalRefs(0),
          mLang(nsIProgrammingLanguage::CPLUSPLUS),
          mRefCount(0),
          mFirstChild(),
          mLastChild()
#ifdef DEBUG_CC
        , mBytes(0),
          mName(nsnull)
#endif
    {
    }

    // Allow uninitialized values in large arrays.
    PtrInfo() {}
};

/**
 * A structure designed to be used like a linked list of PtrInfo, except
 * that allocates the PtrInfo 32K-at-a-time.
 */
class NodePool
{
private:
    enum { BlockSize = 32 * 1024 }; // could be int template parameter

    struct Block {
        Block* mNext;
        PtrInfo mEntries[BlockSize];

        Block() : mNext(nsnull) {}
    };

public:
    NodePool()
        : mBlocks(nsnull),
          mLast(nsnull)
    {
    }

    ~NodePool()
    {
        Block *b = mBlocks;
        while (b) {
            Block *n = b->mNext;
            delete b;
            b = n;
        }
    }

    class Builder;
    friend class Builder;
    class Builder {
    public:
        Builder(NodePool& aPool)
            : mNextBlock(&aPool.mBlocks),
              mNext(aPool.mLast),
              mBlockEnd(nsnull)
        {
            NS_ASSERTION(aPool.mBlocks == nsnull && aPool.mLast == nsnull,
                         "pool not empty");
        }
        PtrInfo *Add(void *aPointer)
        {
            if (mNext == mBlockEnd) {
                Block *block;
                if (!(*mNextBlock = block = new Block()))
                    return nsnull;
                mNext = block->mEntries;
                mBlockEnd = block->mEntries + BlockSize;
                mNextBlock = &block->mNext;
            }
            return new (mNext++) PtrInfo(aPointer);
        }
    private:
        Block **mNextBlock;
        PtrInfo *&mNext;
        PtrInfo *mBlockEnd;
    };

    class Enumerator;
    friend class Enumerator;
    class Enumerator {
    public:
        Enumerator(NodePool& aPool)
            : mFirstBlock(aPool.mBlocks),
              mCurBlock(nsnull),
              mNext(nsnull),
              mBlockEnd(nsnull),
              mLast(aPool.mLast)
        {
        }

        PRBool IsDone() const
        {
            return mNext == mLast;
        }

        PtrInfo* GetNext()
        {
            NS_ASSERTION(!IsDone(), "calling GetNext when done");
            if (mNext == mBlockEnd) {
                Block *nextBlock = mCurBlock ? mCurBlock->mNext : mFirstBlock;
                mNext = nextBlock->mEntries;
                mBlockEnd = mNext + BlockSize;
                mCurBlock = nextBlock;
            }
            return mNext++;
        }
    private:
        Block *mFirstBlock, *mCurBlock;
        // mNext is the next value we want to return, unless mNext == mBlockEnd
        // NB: mLast is a reference to allow enumerating while building!
        PtrInfo *mNext, *mBlockEnd, *&mLast;
    };

private:
    Block *mBlocks;
    PtrInfo *mLast;
};

struct GCGraph
{
    NodePool mNodes;
    EdgePool mEdges;
    PRUint32 mRootCount;

    GCGraph() : mRootCount(0) {}
};

// XXX Would be nice to have an nsHashSet<KeyType> API that has
// Add/Remove/Has rather than PutEntry/RemoveEntry/GetEntry.
typedef nsTHashtable<nsVoidPtrHashKey> PointerSet;
typedef nsBaseHashtable<nsVoidPtrHashKey, PRUint32, PRUint32>
    PointerSetWithGeneration;

static void
Fault(const char *msg, const void *ptr);

struct nsPurpleBuffer
{

#define ASSOCIATIVITY 2
#define INDEX_LOW_BIT 6
#define N_INDEX_BITS 13

#define N_ENTRIES (1 << N_INDEX_BITS)
#define N_POINTERS (N_ENTRIES * ASSOCIATIVITY)
#define TOTAL_BYTES (N_POINTERS * PR_BYTES_PER_WORD)
#define INDEX_MASK PR_BITMASK(N_INDEX_BITS)
#define POINTER_INDEX(P) ((((PRUword)P) >> INDEX_LOW_BIT) & (INDEX_MASK))

#if (INDEX_LOW_BIT + N_INDEX_BITS > (8 * PR_BYTES_PER_WORD))
#error "index bit overflow"
#endif

    // This class serves as a generational wrapper around a pldhash
    // table: a subset of generation zero lives in mCache, the
    // remainder spill into the mBackingStore hashtable. The idea is
    // to get a higher hit rate and greater locality of reference for
    // generation zero, in which the vast majority of suspect/forget
    // calls annihilate one another.

    nsCycleCollectorParams &mParams;
#ifdef DEBUG_CC
    nsCycleCollectorStats &mStats;
#endif
    void* mCache[N_POINTERS];
    PRUint32 mCurrGen;    
    PointerSetWithGeneration mBackingStore;
    nsDeque *mTransferBuffer;
    
#ifdef DEBUG_CC
    nsPurpleBuffer(nsCycleCollectorParams &params,
                   nsCycleCollectorStats &stats) 
        : mParams(params),
          mStats(stats),
          mCurrGen(0),
          mTransferBuffer(nsnull)
    {
        Init();
    }
#else
    nsPurpleBuffer(nsCycleCollectorParams &params) 
        : mParams(params),
          mCurrGen(0),
          mTransferBuffer(nsnull)
    {
        Init();
    }
#endif

    ~nsPurpleBuffer()
    {
        memset(mCache, 0, sizeof(mCache));
        mBackingStore.Clear();
    }

    void Init()
    {
        memset(mCache, 0, sizeof(mCache));
        mBackingStore.Init();
    }

    void BumpGeneration();
    void SelectAgedPointers(nsDeque *transferBuffer);

    PRBool Exists(void *p)
    {
        PRUint32 idx = POINTER_INDEX(p);
        for (PRUint32 i = 0; i < ASSOCIATIVITY; ++i) {
            if (mCache[idx+i] == p)
                return PR_TRUE;
        }
        PRUint32 gen;
        return mBackingStore.Get(p, &gen);
    }

    void Put(void *p)
    {
        PRUint32 idx = POINTER_INDEX(p);
        for (PRUint32 i = 0; i < ASSOCIATIVITY; ++i) {
            if (!mCache[idx+i]) {
                mCache[idx+i] = p;
                return;
            }
        }
#ifdef DEBUG_CC
        mStats.mSpills++;
#endif
        SpillOne(p);
    }

    void Remove(void *p)     
    {
        PRUint32 idx = POINTER_INDEX(p);
        for (PRUint32 i = 0; i < ASSOCIATIVITY; ++i) {
            if (mCache[idx+i] == p) {
                mCache[idx+i] = (void*)0;
                return;
            }
        }
        mBackingStore.Remove(p);
    }

    void SpillOne(void* &p)
    {
        mBackingStore.Put(p, mCurrGen);
        p = (void*)0;
    }

    void SpillAll()
    {
        for (PRUint32 i = 0; i < N_POINTERS; ++i) {
            if (mCache[i]) {
                SpillOne(mCache[i]);
            }
        }
    }
};

static PR_CALLBACK PLDHashOperator
zeroGenerationCallback(const void*  ptr,
                       PRUint32&    generation,
                       void*        userArg)
{
#ifdef DEBUG_CC
    nsPurpleBuffer *purp = NS_STATIC_CAST(nsPurpleBuffer*, userArg);
    purp->mStats.mZeroGeneration++;
#endif
    generation = 0;
    return PL_DHASH_NEXT;
}

void nsPurpleBuffer::BumpGeneration()
{
    SpillAll();
    if (mCurrGen == 0xffffffff) {
        mBackingStore.Enumerate(zeroGenerationCallback, this);
        mCurrGen = 0;
    } else {
        ++mCurrGen;
    }
#ifdef DEBUG_CC
    mStats.mBumpGeneration++;
#endif
}

static inline PRBool
SufficientlyAged(PRUint32 generation, nsPurpleBuffer *p)
{
    return generation + p->mParams.mScanDelay < p->mCurrGen;
}

static PR_CALLBACK PLDHashOperator
ageSelectionCallback(const void*  ptr,
                     PRUint32&    generation,
                     void*        userArg)
{
    nsPurpleBuffer *purp = NS_STATIC_CAST(nsPurpleBuffer*, userArg);
    if (SufficientlyAged(generation, purp)) {
        nsISupports *root = NS_STATIC_CAST(nsISupports *, 
                                           NS_CONST_CAST(void*, ptr));
        purp->mTransferBuffer->Push(root);
    }
    return PL_DHASH_NEXT;
}

void
nsPurpleBuffer::SelectAgedPointers(nsDeque *transferBuffer)
{
    mTransferBuffer = transferBuffer;
    mBackingStore.Enumerate(ageSelectionCallback, this);
    mTransferBuffer = nsnull;
}



////////////////////////////////////////////////////////////////////////
// Implement the LanguageRuntime interface for C++/XPCOM 
////////////////////////////////////////////////////////////////////////


struct nsCycleCollectionXPCOMRuntime : 
    public nsCycleCollectionLanguageRuntime 
{
    nsresult BeginCycleCollection() 
    {
        return NS_OK;
    }

    nsresult Traverse(void *p, nsCycleCollectionTraversalCallback &cb) 
    {
        nsresult rv;

        nsISupports *s = NS_STATIC_CAST(nsISupports *, p);        
        nsCycleCollectionParticipant *cp;
        ToParticipant(s, &cp);
        if (!cp) {
            Fault("walking wrong type of pointer", s);
            return NS_ERROR_FAILURE;
        }

        rv = cp->Traverse(s, cb);
        if (NS_FAILED(rv)) {
            Fault("XPCOM pointer traversal failed", s);
            return NS_ERROR_FAILURE;
        }
        return NS_OK;
    }

    nsresult Root(const nsDeque &nodes)
    {
        for (PRInt32 i = 0; i < nodes.GetSize(); ++i) {
            void *p = nodes.ObjectAt(i);
            nsISupports *s = NS_STATIC_CAST(nsISupports *, p);
            NS_ADDREF(s);
        }
        return NS_OK;
    }

    nsresult Unlink(const nsDeque &nodes)
    {
        nsresult rv;

        for (PRInt32 i = 0; i < nodes.GetSize(); ++i) {
            void *p = nodes.ObjectAt(i);

            nsISupports *s = NS_STATIC_CAST(nsISupports *, p);
            nsCycleCollectionParticipant *cp;
            ToParticipant(s, &cp);
            if (!cp) {
                Fault("unlinking wrong kind of pointer", s);
                return NS_ERROR_FAILURE;
            }

            rv = cp->Unlink(s);

            if (NS_FAILED(rv)) {
                Fault("failed unlink", s);
                return NS_ERROR_FAILURE;
            }
        }
        return NS_OK;
    }

    nsresult Unroot(const nsDeque &nodes)
    {
        for (PRInt32 i = 0; i < nodes.GetSize(); ++i) {
            void *p = nodes.ObjectAt(i);
            nsISupports *s = NS_STATIC_CAST(nsISupports *, p);
            NS_RELEASE(s);
        }
        return NS_OK;
    }

    nsresult FinishCycleCollection() 
    {
        return NS_OK;
    }
};

struct nsCycleCollector
{
    PRBool mCollectionInProgress;
    PRBool mScanInProgress;

    nsCycleCollectionLanguageRuntime *mRuntimes[nsIProgrammingLanguage::MAX+1];
    nsCycleCollectionXPCOMRuntime mXPCOMRuntime;

    // The set of buffers |mBufs| serves a variety of purposes; mostly
    // involving the transfer of pointers from a hashtable iterator
    // routine to some outer logic that might also need to mutate the
    // hashtable. In some contexts, only buffer 0 is used (as a
    // set-of-all-pointers); in other contexts, one buffer is used
    // per-language (as a set-of-pointers-in-language-N).

    nsDeque mBufs[nsIProgrammingLanguage::MAX + 1];
    
    nsCycleCollectorParams mParams;

    nsPurpleBuffer mPurpleBuf;

    void RegisterRuntime(PRUint32 langID, 
                         nsCycleCollectionLanguageRuntime *rt);
    void ForgetRuntime(PRUint32 langID);

    void CollectPurple(); // XXXldb Should this be called SelectPurple?
    void MarkRoots(GCGraph &graph);
    void ScanRoots(GCGraph &graph);
    void CollectWhite(GCGraph &graph);

    nsCycleCollector();
    ~nsCycleCollector();

    void Suspect(nsISupports *n, PRBool current = PR_FALSE);
    void Forget(nsISupports *n);
    void Allocated(void *n, size_t sz);
    void Freed(void *n);
    void Collect(PRUint32 aTryCollections = 1);
    void Shutdown();

#ifdef DEBUG_CC
    nsCycleCollectorStats mStats;    

    FILE *mPtrLog;

    void MaybeDrawGraphs(GCGraph &graph);
    void ExplainLiveExpectedGarbage();
    void ShouldBeFreed(nsISupports *n);
    void WasFreed(nsISupports *n);
    PointerSet mExpectedGarbage;
#endif
};


class GraphWalker
{
private:
    void DoWalk(nsDeque &aQueue);

public:
    void Walk(PtrInfo *s0);
    void WalkFromRoots(GCGraph &aGraph);

    // Provided by concrete walker subtypes.
    virtual PRBool ShouldVisitNode(PtrInfo const *pi) = 0;
    virtual void VisitNode(PtrInfo *pi) = 0;
};


////////////////////////////////////////////////////////////////////////
// The static collector object
////////////////////////////////////////////////////////////////////////


static nsCycleCollector *sCollector = nsnull;


////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////

static void
Fault(const char *msg, const void *ptr=nsnull)
{
#ifdef DEBUG_CC
    // This should be nearly impossible, but just in case.
    if (!sCollector)
        return;

    if (sCollector->mParams.mFaultIsFatal) {

        if (ptr)
            printf("Fatal fault in cycle collector: %s (ptr: %p)\n", msg, ptr);
        else
            printf("Fatal fault in cycle collector: %s\n", msg);

        exit(1);

    } 
#endif

    NS_NOTREACHED(nsPrintfCString(256,
                  "Fault in cycle collector: %s (ptr: %p)\n",
                  msg, ptr).get());

    // When faults are not fatal, we assume we're running in a
    // production environment and we therefore want to disable the
    // collector on a fault. This will unfortunately cause the browser
    // to leak pretty fast wherever creates cyclical garbage, but it's
    // probably a better user experience than crashing. Besides, we
    // *should* never hit a fault.

    sCollector->mParams.mDoNothing = PR_TRUE;
}



static nsISupports *
canonicalize(nsISupports *in)
{
    nsCOMPtr<nsISupports> child;
    in->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                       getter_AddRefs(child));
    return child.get();
}


void
GraphWalker::Walk(PtrInfo *s0)
{
    nsDeque queue;
    queue.Push(s0);
    DoWalk(queue);
}

void
GraphWalker::WalkFromRoots(GCGraph& aGraph)
{
    nsDeque queue;
    NodePool::Enumerator etor(aGraph.mNodes);
    for (PRUint32 i = 0; i < aGraph.mRootCount; ++i) {
        queue.Push(etor.GetNext());
    }
    DoWalk(queue);
}

void
GraphWalker::DoWalk(nsDeque &aQueue)
{
    // Use a aQueue to match the breadth-first traversal used when we
    // built the graph, for hopefully-better locality.
    while (aQueue.GetSize() > 0) {
        PtrInfo *pi = NS_STATIC_CAST(PtrInfo*, aQueue.PopFront());

        if (this->ShouldVisitNode(pi)) {
            this->VisitNode(pi);
            for (EdgePool::Iterator child = pi->mFirstChild,
                                child_end = pi->mLastChild;
                 child != child_end; ++child) {
                aQueue.Push(*child);
            }
        }
    };

#ifdef DEBUG_CC
    sCollector->mStats.mWalkedGraph++;
#endif
}


////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |MarkRoots| routine.
////////////////////////////////////////////////////////////////////////

struct PtrToNodeEntry : public PLDHashEntryHdr
{
    // The key is mNode->mPointer
    PtrInfo *mNode;
};

PR_STATIC_CALLBACK(PRBool)
PtrToNodeMatchEntry(PLDHashTable *table,
                    const PLDHashEntryHdr *entry,
                    const void *key)
{
    const PtrToNodeEntry *n = NS_STATIC_CAST(const PtrToNodeEntry*, entry);
    return n->mNode->mPointer == key;
}

static PLDHashTableOps PtrNodeOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashVoidPtrKeyStub,
    PtrToNodeMatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull
};

class GCGraphBuilder : private nsCycleCollectionTraversalCallback
{
private:
    NodePool::Builder mNodeBuilder;
    EdgePool::Builder mEdgeBuilder;
    PLDHashTable mPtrToNodeMap;
    PtrInfo *mCurrPi;
    nsCycleCollectionLanguageRuntime **mRuntimes; // weak, from nsCycleCollector

public:
    GCGraphBuilder(GCGraph &aGraph,
                   nsCycleCollectionLanguageRuntime **aRuntimes);
    ~GCGraphBuilder();

    PRUint32 Count() const { return mPtrToNodeMap.entryCount; }

    PtrInfo* AddNode(void *s);
    void Traverse(PtrInfo* aPtrInfo);

private:
    // nsCycleCollectionTraversalCallback methods.
#ifdef DEBUG_CC
    void DescribeNode(size_t refCount, size_t objSz, const char *objName);
#else
    void DescribeNode(size_t refCount);
#endif
    void NoteXPCOMChild(nsISupports *child);
    void NoteScriptChild(PRUint32 langID, void *child);
};

GCGraphBuilder::GCGraphBuilder(GCGraph &aGraph,
                               nsCycleCollectionLanguageRuntime **aRuntimes)
    : mNodeBuilder(aGraph.mNodes),
      mEdgeBuilder(aGraph.mEdges),
      mRuntimes(aRuntimes)
{
    if (!PL_DHashTableInit(&mPtrToNodeMap, &PtrNodeOps, nsnull,
                           sizeof(PtrToNodeEntry), 32768))
        mPtrToNodeMap.ops = nsnull;
}

GCGraphBuilder::~GCGraphBuilder()
{
    if (mPtrToNodeMap.ops)
        PL_DHashTableFinish(&mPtrToNodeMap);
}

PtrInfo*
GCGraphBuilder::AddNode(void *s)
{
    PtrToNodeEntry *e = NS_STATIC_CAST(PtrToNodeEntry*, 
        PL_DHashTableOperate(&mPtrToNodeMap, s, PL_DHASH_ADD));
    PtrInfo *result;
    if (!e->mNode) {
        // New entry.
        result = mNodeBuilder.Add(s);
        if (!result) {
            PL_DHashTableRawRemove(&mPtrToNodeMap, e);
            return nsnull;
        }
        e->mNode = result;
    } else {
        result = e->mNode;
    }
    return result;
}

void
GCGraphBuilder::Traverse(PtrInfo* aPtrInfo)
{
    mCurrPi = aPtrInfo;

#ifdef DEBUG_CC
    if (mCurrPi->mLang > nsIProgrammingLanguage::MAX ) {
        Fault("unknown language during walk");
        return;
    }

    if (!mRuntimes[mCurrPi->mLang]) {
        Fault("script pointer for unregistered language");
        return;
    }
#endif

    mCurrPi->mFirstChild = mEdgeBuilder.Mark();
    
    nsresult rv =
        mRuntimes[aPtrInfo->mLang]->Traverse(aPtrInfo->mPointer, *this);
    if (NS_FAILED(rv)) {
        Fault("script pointer traversal failed", aPtrInfo->mPointer);
    }

    mCurrPi->mLastChild = mEdgeBuilder.Mark();
}

void 
#ifdef DEBUG_CC
GCGraphBuilder::DescribeNode(size_t refCount, size_t objSz, const char *objName)
#else
GCGraphBuilder::DescribeNode(size_t refCount)
#endif
{
    if (refCount == 0)
        Fault("zero refcount", mCurrPi->mPointer);

#ifdef DEBUG_CC
    mCurrPi->mBytes = objSz;
    mCurrPi->mName = objName;
#endif

    mCurrPi->mRefCount = refCount;
#ifdef DEBUG_CC
    sCollector->mStats.mVisitedNode++;
    if (mCurrPi->mLang == nsIProgrammingLanguage::JAVASCRIPT)
        sCollector->mStats.mVisitedJSNode++;
#endif
}

void 
GCGraphBuilder::NoteXPCOMChild(nsISupports *child) 
{
    if (!child)
        return; 
   
    child = canonicalize(child);

    PRBool scanSafe = nsCycleCollector_isScanSafe(child);
#ifdef DEBUG_CC
    scanSafe &= !nsCycleCollector_shouldSuppress(child);
#endif
    if (scanSafe) {
        PtrInfo *childPi = AddNode(child);
        if (!childPi)
            return;
        mEdgeBuilder.Add(childPi);
        ++childPi->mInternalRefs;
    }
}

void
GCGraphBuilder::NoteScriptChild(PRUint32 langID, void *child) 
{
    if (!child)
        return;

    if (langID > nsIProgrammingLanguage::MAX || !mRuntimes[langID]) {
        Fault("traversing pointer for unregistered language", child);
        return;
    }

    PtrInfo *childPi = AddNode(child);
    if (!childPi)
        return;
    mEdgeBuilder.Add(childPi);
    ++childPi->mInternalRefs;
    childPi->mLang = langID;
}


void 
nsCycleCollector::CollectPurple()
{
    mPurpleBuf.SelectAgedPointers(&mBufs[0]);
}

void
nsCycleCollector::MarkRoots(GCGraph &graph)
{
    if (mBufs[0].GetSize() == 0)
        return;

    GCGraphBuilder builder(graph, mRuntimes);

    int i;
    for (i = 0; i < mBufs[0].GetSize(); ++i) {
        nsISupports *s = NS_STATIC_CAST(nsISupports *, mBufs[0].ObjectAt(i));
        PtrInfo *pi = builder.AddNode(canonicalize(s));
    }

    graph.mRootCount = builder.Count();

    // read the PtrInfo out of the graph that we are building
    NodePool::Enumerator queue(graph.mNodes);
    while (!queue.IsDone()) {
        PtrInfo *pi = queue.GetNext();
        builder.Traverse(pi);
    }
}


////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |ScanRoots| routine.
////////////////////////////////////////////////////////////////////////


struct ScanBlackWalker : public GraphWalker
{
    PRBool ShouldVisitNode(PtrInfo const *pi)
    { 
        return pi->mColor != black;
    }

    void VisitNode(PtrInfo *pi)
    { 
        pi->mColor = black;
#ifdef DEBUG_CC
        sCollector->mStats.mSetColorBlack++;
#endif
    }
};


struct scanWalker : public GraphWalker
{
    PRBool ShouldVisitNode(PtrInfo const *pi)
    { 
        return pi->mColor == grey;
    }

    void VisitNode(PtrInfo *pi)
    {
        if (pi->mColor != grey)
            Fault("scanning non-grey node", pi->mPointer);

        if (pi->mInternalRefs > pi->mRefCount)
            Fault("traversed refs exceed refcount", pi->mPointer);

        if (pi->mInternalRefs == pi->mRefCount) {
            pi->mColor = white;
#ifdef DEBUG_CC
            sCollector->mStats.mSetColorWhite++;
#endif
        } else {
            ScanBlackWalker().Walk(pi);
            NS_ASSERTION(pi->mColor == black,
                         "Why didn't ScanBlackWalker make pi black?");
        }
    }
};

void
nsCycleCollector::ScanRoots(GCGraph &graph)
{
    // On the assumption that most nodes will be black, it's
    // probably faster to use a GraphWalker than a
    // NodePool::Enumerator.
    scanWalker().WalkFromRoots(graph); 

#ifdef DEBUG_CC
    // Sanity check: scan should have colored all grey nodes black or
    // white. So we ensure we have no grey nodes at this point.
    NodePool::Enumerator etor(graph.mNodes);
    while (!etor.IsDone())
    {
        PtrInfo *pinfo = etor.GetNext();
        if (pinfo->mColor == grey) {
            Fault("valid grey node after scanning", pinfo->mPointer);
        }
    }
#endif
}


////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |CollectWhite| routine, somewhat modified.
////////////////////////////////////////////////////////////////////////

void
nsCycleCollector::CollectWhite(GCGraph &graph)
{
    // Explanation of "somewhat modified": we have no way to collect the
    // set of whites "all at once", we have to ask each of them to drop
    // their outgoing links and assume this will cause the garbage cycle
    // to *mostly* self-destruct (except for the reference we continue
    // to hold). 
    // 
    // To do this "safely" we must make sure that the white nodes we're
    // operating on are stable for the duration of our operation. So we
    // make 3 sets of calls to language runtimes:
    //
    //   - Root(whites), which should pin the whites in memory.
    //   - Unlink(whites), which drops outgoing links on each white.
    //   - Unroot(whites), which returns the whites to normal GC.

    PRUint32 i;
    nsresult rv;

    for (i = 0; i < nsIProgrammingLanguage::MAX+1; ++i)
        mBufs[i].Empty();

#if defined(DEBUG_CC) && !defined(__MINGW32__) && defined(WIN32)
    struct _CrtMemState ms1, ms2;
    _CrtMemCheckpoint(&ms1);
#endif

    NodePool::Enumerator etor(graph.mNodes);
    while (!etor.IsDone())
    {
        PtrInfo *pinfo = etor.GetNext();
        void *p = pinfo->mPointer;

        NS_ASSERTION(pinfo->mLang == nsIProgrammingLanguage::CPLUSPLUS ||
                     !mPurpleBuf.Exists(p),
                     "Need to remove non-CPLUSPLUS objects from purple buffer!");
        if (pinfo->mColor == white) {
            if (pinfo->mLang > nsIProgrammingLanguage::MAX)
                Fault("White node has bad language ID", p);
            else
                mBufs[pinfo->mLang].Push(p);

            if (pinfo->mLang == nsIProgrammingLanguage::CPLUSPLUS) {
                nsISupports* s = NS_STATIC_CAST(nsISupports*, p);
                Forget(s);
            }
        }
        else if (pinfo->mLang == nsIProgrammingLanguage::CPLUSPLUS) {
            nsISupports* s = NS_STATIC_CAST(nsISupports*, p);
            nsCycleCollectionParticipant* cp;
            CallQueryInterface(s, &cp);
            if (cp)
                cp->UnmarkPurple(s);
            Forget(s);
        }
    }

    for (i = 0; i < nsIProgrammingLanguage::MAX+1; ++i) {
        if (mRuntimes[i] &&
            mBufs[i].GetSize() > 0) {
            rv = mRuntimes[i]->Root(mBufs[i]);
            if (NS_FAILED(rv))
                Fault("Failed root call while unlinking");
        }
    }

    for (i = 0; i < nsIProgrammingLanguage::MAX+1; ++i) {
        if (mRuntimes[i] &&
            mBufs[i].GetSize() > 0) {
            rv = mRuntimes[i]->Unlink(mBufs[i]);
            if (NS_FAILED(rv)) {
                Fault("Failed unlink call while unlinking");
#ifdef DEBUG_CC
                mStats.mFailedUnlink++;
#endif
            } else {
#ifdef DEBUG_CC
                mStats.mCollectedNode += mBufs[i].GetSize();
#endif
            }
        }
    }

    for (i = 0; i < nsIProgrammingLanguage::MAX+1; ++i) {
        if (mRuntimes[i] &&
            mBufs[i].GetSize() > 0) {
            rv = mRuntimes[i]->Unroot(mBufs[i]);
            if (NS_FAILED(rv))
                Fault("Failed unroot call while unlinking");
        }
    }

    for (i = 0; i < nsIProgrammingLanguage::MAX+1; ++i)
        mBufs[i].Empty();

#if defined(DEBUG_CC) && !defined(__MINGW32__) && defined(WIN32)
    _CrtMemCheckpoint(&ms2);
    if (ms2.lTotalCount < ms1.lTotalCount)
        mStats.mFreedBytes += (ms1.lTotalCount - ms2.lTotalCount);
#endif
}


#ifdef DEBUG_CC
////////////////////////////////////////////////////////////////////////
// Memory-hooking stuff
// When debugging wild pointers, it sometimes helps to hook malloc and
// free. This stuff is disabled unless you set an environment variable.
////////////////////////////////////////////////////////////////////////

static PRBool hookedMalloc = PR_FALSE;

#ifdef __GLIBC__
#include <malloc.h>

static void* (*old_memalign_hook)(size_t, size_t, const void *);
static void* (*old_realloc_hook)(void *, size_t, const void *);
static void* (*old_malloc_hook)(size_t, const void *);
static void (*old_free_hook)(void *, const void *);

static void* my_memalign_hook(size_t, size_t, const void *);
static void* my_realloc_hook(void *, size_t, const void *);
static void* my_malloc_hook(size_t, const void *);
static void my_free_hook(void *, const void *);

static inline void 
install_old_hooks()
{
    __memalign_hook = old_memalign_hook;
    __realloc_hook = old_realloc_hook;
    __malloc_hook = old_malloc_hook;
    __free_hook = old_free_hook;
}

static inline void 
save_old_hooks()
{
    // Glibc docs recommend re-saving old hooks on
    // return from recursive calls. Strangely when 
    // we do this, we find ourselves in infinite
    // recursion.

    //     old_memalign_hook = __memalign_hook;
    //     old_realloc_hook = __realloc_hook;
    //     old_malloc_hook = __malloc_hook;
    //     old_free_hook = __free_hook;
}

static inline void 
install_new_hooks()
{
    __memalign_hook = my_memalign_hook;
    __realloc_hook = my_realloc_hook;
    __malloc_hook = my_malloc_hook;
    __free_hook = my_free_hook;
}

static void*
my_realloc_hook(void *ptr, size_t size, const void *caller)
{
    void *result;    

    install_old_hooks();
    result = realloc(ptr, size);
    save_old_hooks();

    if (sCollector) {
        sCollector->Freed(ptr);
        sCollector->Allocated(result, size);
    }

    install_new_hooks();

    return result;
}


static void* 
my_memalign_hook(size_t size, size_t alignment, const void *caller)
{
    void *result;    

    install_old_hooks();
    result = memalign(size, alignment);
    save_old_hooks();

    if (sCollector)
        sCollector->Allocated(result, size);

    install_new_hooks();

    return result;
}


static void 
my_free_hook (void *ptr, const void *caller)
{
    install_old_hooks();
    free(ptr);
    save_old_hooks();

    if (sCollector)
        sCollector->Freed(ptr);

    install_new_hooks();
}      


static void*
my_malloc_hook (size_t size, const void *caller)
{
    void *result;

    install_old_hooks();
    result = malloc (size);
    save_old_hooks();

    if (sCollector)
        sCollector->Allocated(result, size);

    install_new_hooks();

    return result;
}


static void 
InitMemHook(void)
{
    if (!hookedMalloc) {
        save_old_hooks();
        install_new_hooks();
        hookedMalloc = PR_TRUE;        
    }
}

#elif defined(WIN32)
#ifndef __MINGW32__

static int 
AllocHook(int allocType, void *userData, size_t size, int 
          blockType, long requestNumber, const unsigned char *filename, int 
          lineNumber)
{
    if (allocType == _HOOK_FREE)
        sCollector->Freed(userData);
    return 1;
}


static void InitMemHook(void)
{
    if (!hookedMalloc) {
        _CrtSetAllocHook (AllocHook);
        hookedMalloc = PR_TRUE;        
    }
}
#endif // __MINGW32__

#elif 0 // defined(XP_MACOSX)

#include <malloc/malloc.h>

static void (*old_free)(struct _malloc_zone_t *zone, void *ptr);

static void
freehook(struct _malloc_zone_t *zone, void *ptr)
{
    if (sCollector)
        sCollector->Freed(ptr);
    old_free(zone, ptr);
}


static void
InitMemHook(void)
{
    if (!hookedMalloc) {
        malloc_zone_t *default_zone = malloc_default_zone();
        old_free = default_zone->free;
        default_zone->free = freehook;
        hookedMalloc = PR_TRUE;
    }
}


#else

static void
InitMemHook(void)
{
}

#endif // GLIBC / WIN32 / OSX
#endif // DEBUG_CC

////////////////////////////////////////////////////////////////////////
// Collector implementation
////////////////////////////////////////////////////////////////////////

nsCycleCollector::nsCycleCollector() : 
    mCollectionInProgress(PR_FALSE),
    mScanInProgress(PR_FALSE),
#ifdef DEBUG_CC
    mPurpleBuf(mParams, mStats),
    mPtrLog(nsnull)
#else
    mPurpleBuf(mParams)
#endif
{
#ifdef DEBUG_CC
    mExpectedGarbage.Init();
#endif

    memset(mRuntimes, 0, sizeof(mRuntimes));
    mRuntimes[nsIProgrammingLanguage::CPLUSPLUS] = &mXPCOMRuntime;
}


nsCycleCollector::~nsCycleCollector()
{
    for (PRUint32 i = 0; i < nsIProgrammingLanguage::MAX+1; ++i) {
        mRuntimes[i] = NULL;
    }
}


void 
nsCycleCollector::RegisterRuntime(PRUint32 langID, 
                                  nsCycleCollectionLanguageRuntime *rt)
{
    if (mParams.mDoNothing)
        return;

    if (langID > nsIProgrammingLanguage::MAX)
        Fault("unknown language runtime in registration");

    if (mRuntimes[langID])
        Fault("multiple registrations of language runtime", rt);

    mRuntimes[langID] = rt;
}


void 
nsCycleCollector::ForgetRuntime(PRUint32 langID)
{
    if (mParams.mDoNothing)
        return;

    if (langID > nsIProgrammingLanguage::MAX)
        Fault("unknown language runtime in deregistration");

    if (! mRuntimes[langID])
        Fault("forgetting non-registered language runtime");

    mRuntimes[langID] = nsnull;
}


#ifdef DEBUG_CC
void 
nsCycleCollector::MaybeDrawGraphs(GCGraph &graph)
{
    if (mParams.mDrawGraphs) {
        // We draw graphs only if there were any white nodes.
        PRBool anyWhites = PR_FALSE;
        NodePool::Enumerator fwetor(graph.mNodes);
        while (!fwetor.IsDone())
        {
            PtrInfo *pinfo = fwetor.GetNext();
            if (pinfo->mColor == white) {
                anyWhites = PR_TRUE;
                break;
            }
        }

        if (anyWhites) {
            // We can't just use _popen here because graphviz-for-windows
            // doesn't set up its stdin stream properly, sigh.
            FILE *stream;
#ifdef WIN32
            stream = fopen("c:\\cycle-graph.dot", "w+");
#else
            stream = popen("dotty -", "w");
#endif
            fprintf(stream, 
                    "digraph collection {\n"
                    "rankdir=LR\n"
                    "node [fontname=fixed, fontsize=10, style=filled, shape=box]\n"
                    );

            NodePool::Enumerator etor(graph.mNodes);
            while (!etor.IsDone()) {
                PtrInfo *pi = etor.GetNext();
                const void *p = pi->mPointer;
                fprintf(stream, 
                        "n%p [label=\"%s\\n%p\\n%u/%u refs found\", "
                        "fillcolor=%s, fontcolor=%s]\n", 
                        p,
                        pi->mName,
                        p,
                        pi->mInternalRefs, pi->mRefCount,
                        (pi->mColor == black ? "black" : "white"),
                        (pi->mColor == black ? "white" : "black"));
                for (EdgePool::Iterator child = pi->mFirstChild,
                                    child_end = pi->mLastChild;
                     child != child_end; ++child) {
                    fprintf(stream, "n%p -> n%p\n", p, (*child)->mPointer);
                }
            }

            fprintf(stream, "\n}\n");
#ifdef WIN32
            fclose(stream);
            // Even dotty doesn't work terribly well on windows, since
            // they execute lefty asynchronously. So we'll just run 
            // lefty ourselves.
            _spawnlp(_P_WAIT, 
                     "lefty", 
                     "lefty",
                     "-e",
                     "\"load('dotty.lefty');"
                     "dotty.simple('c:\\cycle-graph.dot');\"",
                     NULL);
            unlink("c:\\cycle-graph.dot");
#else
            pclose(stream);
#endif
        }
    }
}

class Suppressor :
    public nsCycleCollectionTraversalCallback
{
protected:
    static char *sSuppressionList;
    static PRBool sInitialized;
    PRBool mSuppressThisNode;
public:
    Suppressor()
    {
    }

    PRBool shouldSuppress(nsISupports *s)
    {
        if (!sInitialized) {
            sSuppressionList = PR_GetEnv("XPCOM_CC_SUPPRESS");
            sInitialized = PR_TRUE;
        }
        if (sSuppressionList == nsnull) {
            mSuppressThisNode = PR_FALSE;
        } else {
            nsresult rv;
            nsCOMPtr<nsCycleCollectionParticipant> cp = do_QueryInterface(s, &rv);
            if (NS_FAILED(rv)) {
                Fault("checking suppression on wrong type of pointer", s);
                return PR_TRUE;
            }
            cp->Traverse(s, *this);
        }
        return mSuppressThisNode;
    }

    void DescribeNode(size_t refCount, size_t objSz, const char *objName)
    {
        mSuppressThisNode = (PL_strstr(sSuppressionList, objName) != nsnull);
    }

    void NoteXPCOMChild(nsISupports *child) {}
    void NoteScriptChild(PRUint32 langID, void *child) {}
};

char *Suppressor::sSuppressionList = nsnull;
PRBool Suppressor::sInitialized = PR_FALSE;

static PRBool
nsCycleCollector_shouldSuppress(nsISupports *s)
{
    Suppressor supp;
    return supp.shouldSuppress(s);
}
#endif

void 
nsCycleCollector::Suspect(nsISupports *n, PRBool current)
{
    // Re-entering ::Suspect during collection used to be a fault, but
    // we are canonicalizing nsISupports pointers using QI, so we will
    // see some spurious refcount traffic here. 

    if (mScanInProgress)
        return;

    NS_ASSERTION(nsCycleCollector_isScanSafe(n),
                 "suspected a non-scansafe pointer");
    NS_ASSERTION(NS_IsMainThread(), "trying to suspect from non-main thread");

    if (mParams.mDoNothing)
        return;

#ifdef DEBUG_CC
    mStats.mSuspectNode++;

    if (nsCycleCollector_shouldSuppress(n))
        return;

#ifndef __MINGW32__
    if (mParams.mHookMalloc)
        InitMemHook();
#endif

    if (mParams.mLogPointers) {
        if (!mPtrLog)
            mPtrLog = fopen("pointer_log", "w");
        fprintf(mPtrLog, "S %p\n", NS_STATIC_CAST(void*, n));
    }
#endif

    if (current)
        mBufs[0].Push(n);
    else
        mPurpleBuf.Put(n);
}


void 
nsCycleCollector::Forget(nsISupports *n)
{
    // Re-entering ::Forget during collection used to be a fault, but
    // we are canonicalizing nsISupports pointers using QI, so we will
    // see some spurious refcount traffic here. 

    if (mScanInProgress)
        return;

    NS_ASSERTION(NS_IsMainThread(), "trying to forget from non-main thread");
    
    if (mParams.mDoNothing)
        return;

#ifdef DEBUG_CC
    mStats.mForgetNode++;

#ifndef __MINGW32__
    if (mParams.mHookMalloc)
        InitMemHook();
#endif

    if (mParams.mLogPointers) {
        if (!mPtrLog)
            mPtrLog = fopen("pointer_log", "w");
        fprintf(mPtrLog, "F %p\n", NS_STATIC_CAST(void*, n));
    }
#endif

    mPurpleBuf.Remove(n);
}

#ifdef DEBUG_CC
void 
nsCycleCollector::Allocated(void *n, size_t sz)
{
}

void 
nsCycleCollector::Freed(void *n)
{
    mStats.mFreeCalls++;

    if (!n) {
        // Ignore null pointers coming through
        return;
    }

    if (mPurpleBuf.Exists(n)) {
        mStats.mForgetNode++;
        mStats.mFreedWhilePurple++;
        Fault("freed while purple", n);
        mPurpleBuf.Remove(n);
        
        if (mParams.mLogPointers) {
            if (!mPtrLog)
                mPtrLog = fopen("pointer_log", "w");
            fprintf(mPtrLog, "R %p\n", n);
        }
    }
}
#endif

void
nsCycleCollector::Collect(PRUint32 aTryCollections)
{
#if defined(DEBUG_CC) && !defined(__MINGW32__)
    if (!mParams.mDoNothing && mParams.mHookMalloc)
        InitMemHook();
#endif

#ifdef COLLECT_TIME_DEBUG
    printf("cc: Starting nsCycleCollector::Collect(%d)\n", aTryCollections);
    PRTime start = PR_Now(), now;
#endif

    while (aTryCollections > 0) {
        // This triggers a JS GC. Our caller assumes we always trigger at
        // least one JS GC -- they rely on this fact to avoid redundant JS
        // GC calls -- so it's essential that we actually execute this
        // step!
        //
        // It is also essential to empty mBufs[0] here because starting up
        // collection in language runtimes may force some "current" suspects
        // into mBufs[0].
        mBufs[0].Empty();

#ifdef COLLECT_TIME_DEBUG
        now = PR_Now();
#endif
        for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
            if (mRuntimes[i])
                mRuntimes[i]->BeginCycleCollection();
        }

#ifdef COLLECT_TIME_DEBUG
        printf("cc: mRuntimes[*]->BeginCycleCollection() took %lldms\n",
               (PR_Now() - now) / PR_USEC_PER_MSEC);
#endif

        if (mParams.mDoNothing) {
            aTryCollections = 0;
        } else {
#ifdef COLLECT_TIME_DEBUG
            now = PR_Now();
#endif

            CollectPurple();

#ifdef COLLECT_TIME_DEBUG
            printf("cc: CollectPurple() took %lldms\n",
                   (PR_Now() - now) / PR_USEC_PER_MSEC);
#endif

            if (mBufs[0].GetSize() == 0) {
                aTryCollections = 0;
            } else {
                if (mCollectionInProgress)
                    Fault("re-entered collection");

                mCollectionInProgress = PR_TRUE;

                mScanInProgress = PR_TRUE;

                GCGraph graph;

                // The main Bacon & Rajan collection algorithm.

#ifdef COLLECT_TIME_DEBUG
                now = PR_Now();
#endif
                MarkRoots(graph);

#ifdef COLLECT_TIME_DEBUG
                {
                    PRTime then = PR_Now();
                    printf("cc: MarkRoots() took %lldms\n",
                           (then - now) / PR_USEC_PER_MSEC);
                    now = then;
                }
#endif

                ScanRoots(graph);

#ifdef COLLECT_TIME_DEBUG
                printf("cc: ScanRoots() took %lldms\n",
                       (PR_Now() - now) / PR_USEC_PER_MSEC);
#endif

#ifdef DEBUG_CC
                MaybeDrawGraphs(graph);
#endif

                mScanInProgress = PR_FALSE;


#ifdef COLLECT_TIME_DEBUG
                now = PR_Now();
#endif
                CollectWhite(graph);

#ifdef COLLECT_TIME_DEBUG
                printf("cc: CollectWhite() took %lldms\n",
                       (PR_Now() - now) / PR_USEC_PER_MSEC);
#endif

                // Some additional book-keeping.

                --aTryCollections;
            }

            mPurpleBuf.BumpGeneration();

#ifdef DEBUG_CC
            mStats.mCollection++;
            if (mParams.mReportStats)
                mStats.Dump();
#endif

            mCollectionInProgress = PR_FALSE;
        }

        for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
            if (mRuntimes[i])
                mRuntimes[i]->FinishCycleCollection();
        }
    }

#ifdef COLLECT_TIME_DEBUG
    printf("cc: Collect() took %lldms\n",
           (PR_Now() - start) / PR_USEC_PER_MSEC);
#endif
}

void
nsCycleCollector::Shutdown()
{
    // Here we want to run a final collection on everything we've seen
    // buffered, irrespective of age; then permanently disable
    // the collector because the program is shutting down.

    mPurpleBuf.BumpGeneration();
    mParams.mScanDelay = 0;
    Collect(SHUTDOWN_COLLECTIONS(mParams));

#ifdef DEBUG_CC
    CollectPurple();
    if (mBufs[0].GetSize() != 0) {
        printf("Might have been able to release more cycles if the cycle collector would "
               "run once more at shutdown.\n");
    }

    ExplainLiveExpectedGarbage();
#endif
    mParams.mDoNothing = PR_TRUE;
}

#ifdef DEBUG_CC

PR_STATIC_CALLBACK(PLDHashOperator)
AddExpectedGarbage(nsVoidPtrHashKey *p, void *arg)
{
    nsCycleCollector *c = NS_STATIC_CAST(nsCycleCollector*, arg);
    c->mBufs[0].Push(NS_CONST_CAST(void*, p->GetKey()));
    return PL_DHASH_NEXT;
}

void
nsCycleCollector::ExplainLiveExpectedGarbage()
{
    if (mScanInProgress || mCollectionInProgress)
        Fault("can't explain expected garbage during collection itself");

    if (mParams.mDoNothing) {
        printf("nsCycleCollector: not explaining expected garbage since\n"
               "  cycle collection disabled\n");
        return;
    }

    for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
        if (mRuntimes[i])
            mRuntimes[i]->BeginCycleCollection();
    }

    mCollectionInProgress = PR_TRUE;
    mScanInProgress = PR_TRUE;

    {
        GCGraph graph;
        mBufs[0].Empty();

        // Instead of filling mBufs[0] from the purple buffer, we fill it
        // from the list of nodes we were expected to collect.
        mExpectedGarbage.EnumerateEntries(&AddExpectedGarbage, this);

        MarkRoots(graph);
        ScanRoots(graph);

        mScanInProgress = PR_FALSE;

        NodePool::Enumerator queue(graph.mNodes);
        while (!queue.IsDone()) {
            PtrInfo *pi = queue.GetNext();
            if (pi->mColor == white) {
                printf("nsCycleCollector: %s %p was not collected due to\n"
                       "  missing call to suspect or failure to unlink\n",
                       pi->mName, pi->mPointer);
            }

            if (pi->mInternalRefs != pi->mRefCount) {
                // Note that the external references may have been external
                // to a different node in the cycle collection that just
                // happened, if that different node was purple and then
                // black.
                printf("nsCycleCollector: %s %p was not collected due to %d\n"
                       "  external references\n",
                       pi->mName, pi->mPointer,
                       pi->mRefCount - pi->mInternalRefs);
            }
        }
    }

    mCollectionInProgress = PR_FALSE;

    for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
        if (mRuntimes[i])
            mRuntimes[i]->FinishCycleCollection();
    }    
}

void
nsCycleCollector::ShouldBeFreed(nsISupports *n)
{
    mExpectedGarbage.PutEntry(n);
}

void
nsCycleCollector::WasFreed(nsISupports *n)
{
    mExpectedGarbage.RemoveEntry(n);
}
#endif

////////////////////////////////////////////////////////////////////////
// Module public API (exported in nsCycleCollector.h)
// Just functions that redirect into the singleton, once it's built.
////////////////////////////////////////////////////////////////////////

void 
nsCycleCollector_registerRuntime(PRUint32 langID, 
                                 nsCycleCollectionLanguageRuntime *rt)
{
    if (sCollector)
        sCollector->RegisterRuntime(langID, rt);
}


void 
nsCycleCollector_forgetRuntime(PRUint32 langID)
{
    if (sCollector)
        sCollector->ForgetRuntime(langID);
}


void 
nsCycleCollector_suspect(nsISupports *n)
{
    if (sCollector)
        sCollector->Suspect(n);
}


void 
nsCycleCollector_suspectCurrent(nsISupports *n)
{
    if (sCollector)
        sCollector->Suspect(n, PR_TRUE);
}


void 
nsCycleCollector_forget(nsISupports *n)
{
    if (sCollector)
        sCollector->Forget(n);
}


void 
nsCycleCollector_collect()
{
    if (sCollector)
        sCollector->Collect();
}

nsresult 
nsCycleCollector_startup()
{
    NS_ASSERTION(!sCollector, "Forgot to call nsCycleCollector_shutdown?");

    sCollector = new nsCycleCollector();
    return sCollector ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void 
nsCycleCollector_shutdown()
{
    if (sCollector) {
        sCollector->Shutdown();
        delete sCollector;
        sCollector = nsnull;
    }
}

#ifdef DEBUG
void
nsCycleCollector_DEBUG_shouldBeFreed(nsISupports *n)
{
#ifdef DEBUG_CC
    if (sCollector)
        sCollector->ShouldBeFreed(n);
#endif
}

void
nsCycleCollector_DEBUG_wasFreed(nsISupports *n)
{
#ifdef DEBUG_CC
    if (sCollector)
        sCollector->WasFreed(n);
#endif
}
#endif

PRBool
nsCycleCollector_isScanSafe(nsISupports *s)
{
    if (!s)
        return PR_FALSE;

    nsCycleCollectionParticipant *cp;
    ToParticipant(s, &cp);

    return cp != nsnull;
}

static void
ToParticipant(nsISupports *s, nsCycleCollectionParticipant **cp)
{
    // We use QI to move from an nsISupports to an
    // nsCycleCollectionParticipant, which is a per-class singleton helper
    // object that implements traversal and unlinking logic for the nsISupports
    // in question.
    CallQueryInterface(s, cp);
#ifdef DEBUG_CC
    if (cp)
        ++sCollector->mStats.mSuccessfulQI;
    else
        ++sCollector->mStats.mFailedQI;
#endif
}
