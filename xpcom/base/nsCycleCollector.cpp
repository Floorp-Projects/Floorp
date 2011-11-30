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
//  - It can be QI'ed to |nsXPCOMCycleCollectionParticipant|, though this
//    operation loses ISupports identity (like nsIClassInfo).
//  - The operation |traverse| on the resulting
//    nsXPCOMCycleCollectionParticipant does not cause *any* refcount
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

#if !defined(__MINGW32__)
#ifdef WIN32
#include <crtdbg.h>
#include <errno.h>
#endif
#endif

#include "base/process_util.h"

/* This must occur *after* base/process_util.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollectorUtils.h"
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
#include "mozilla/FunctionTimer.h"
#include "nsIObserverService.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "mozilla/Services.h"
#include "nsICycleCollectorListener.h"
#include "nsIXPConnect.h"
#include "nsIJSRuntimeService.h"
#include "nsIMemoryReporter.h"
#include "xpcpublic.h"
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <io.h>
#include <process.h>
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

//#define COLLECT_TIME_DEBUG

#ifdef DEBUG_CC
#define IF_DEBUG_CC_PARAM(_p) , _p
#define IF_DEBUG_CC_ONLY_PARAM(_p) _p
#else
#define IF_DEBUG_CC_PARAM(_p)
#define IF_DEBUG_CC_ONLY_PARAM(_p)
#endif

#define DEFAULT_SHUTDOWN_COLLECTIONS 5
#ifdef DEBUG_CC
#define SHUTDOWN_COLLECTIONS(params) params.mShutdownCollections
#else
#define SHUTDOWN_COLLECTIONS(params) DEFAULT_SHUTDOWN_COLLECTIONS
#endif

#if defined(XP_WIN)
// Defined in nsThreadManager.cpp.
extern DWORD gTLSThreadIDIndex;
#elif defined(NS_TLS)
// Defined in nsThreadManager.cpp.
extern NS_TLS mozilla::threads::ID gTLSThreadID;
#else
PRThread* gCycleCollectorThread = nsnull;
#endif

// If true, always log cycle collector graphs.
const bool gAlwaysLogCCGraphs = false;

// Various parameters of this collector can be tuned using environment
// variables.

struct nsCycleCollectorParams
{
    bool mDoNothing;
    bool mLogGraphs;
#ifdef DEBUG_CC
    bool mReportStats;
    bool mHookMalloc;
    bool mFaultIsFatal;
    bool mLogPointers;
    PRUint32 mShutdownCollections;
#endif
    
    nsCycleCollectorParams() :
#ifdef DEBUG_CC
        mDoNothing     (PR_GetEnv("XPCOM_CC_DO_NOTHING") != NULL),
        mLogGraphs     (gAlwaysLogCCGraphs ||
                        PR_GetEnv("XPCOM_CC_DRAW_GRAPHS") != NULL),
        mReportStats   (PR_GetEnv("XPCOM_CC_REPORT_STATS") != NULL),
        mHookMalloc    (PR_GetEnv("XPCOM_CC_HOOK_MALLOC") != NULL),
        mFaultIsFatal  (PR_GetEnv("XPCOM_CC_FAULT_IS_FATAL") != NULL),
        mLogPointers   (PR_GetEnv("XPCOM_CC_LOG_POINTERS") != NULL),

        mShutdownCollections(DEFAULT_SHUTDOWN_COLLECTIONS)
#else
        mDoNothing     (false),
        mLogGraphs     (gAlwaysLogCCGraphs)
#endif
    {
#ifdef DEBUG_CC
        char *s = PR_GetEnv("XPCOM_CC_SHUTDOWN_COLLECTIONS");
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
    PRUint32 mWalkedGraph;
    PRUint32 mCollectedBytes;
    PRUint32 mFreeCalls;
    PRUint32 mFreedBytes;

    PRUint32 mSetColorGrey;
    PRUint32 mSetColorBlack;
    PRUint32 mSetColorWhite;

    PRUint32 mFailedUnlink;
    PRUint32 mCollectedNode;

    PRUint32 mSuspectNode;
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
        DUMP(mWalkedGraph);
        DUMP(mCollectedBytes);
        DUMP(mFreeCalls);
        DUMP(mFreedBytes);
    
        DUMP(mSetColorGrey);
        DUMP(mSetColorBlack);
        DUMP(mSetColorWhite);
    
        DUMP(mFailedUnlink);
        DUMP(mCollectedNode);
    
        DUMP(mSuspectNode);
        DUMP(mForgetNode);
        DUMP(mFreedWhilePurple);
    
        DUMP(mCollection);
#undef DUMP
    }
};
#endif

#ifdef DEBUG_CC
static bool nsCycleCollector_shouldSuppress(nsISupports *s);
static void InitMemHook(void);
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
        mNumBlocks = 0;
    }

    ~EdgePool()
    {
        NS_ASSERTION(!mSentinelAndBlocks[0].block &&
                     !mSentinelAndBlocks[1].block,
                     "Didn't call Clear()?");
    }

    void Clear()
    {
        Block *b = Blocks();
        while (b) {
            Block *next = b->Next();
            delete b;
            NS_ASSERTION(mNumBlocks > 0,
                         "Expected EdgePool mNumBlocks to be positive.");
            mNumBlocks--;
            b = next;
        }

        mSentinelAndBlocks[0].block = nsnull;
        mSentinelAndBlocks[1].block = nsnull;
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
        enum { BlockSize = 16 * 1024 };

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
    PRUint32 mNumBlocks;

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
        bool operator==(const Iterator& aOther) const
            { return mPointer == aOther.mPointer; }
        bool operator!=(const Iterator& aOther) const
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
              mNextBlockPtr(&aPool.Blocks()),
              mNumBlocks(aPool.mNumBlocks)
        {
        }

        Iterator Mark() { return Iterator(mCurrent); }

        void Add(PtrInfo* aEdge) {
            if (mCurrent == mBlockEnd) {
                Block *b = new Block();
                if (!b) {
                    // This means we just won't collect (some) cycles.
                    NS_NOTREACHED("out of memory, ignoring edges");
                    return;
                }
                *mNextBlockPtr = b;
                mCurrent = b->Start();
                mBlockEnd = b->End();
                mNextBlockPtr = &b->Next();
                mNumBlocks++;
            }
            (mCurrent++)->ptrInfo = aEdge;
        }
    private:
        // mBlockEnd points to space for null sentinel
        PtrInfoOrBlock *mCurrent, *mBlockEnd;
        Block **mNextBlockPtr;
        PRUint32 &mNumBlocks;
    };

    size_t BlocksSize() const {
        return sizeof(Block) * mNumBlocks;
    }

};

#ifdef DEBUG_CC

struct ReversedEdge {
    PtrInfo *mTarget;
    nsCString *mEdgeName;
    ReversedEdge *mNext;
};

#endif


enum NodeColor { black, white, grey };

// This structure should be kept as small as possible; we may expect
// hundreds of thousands of them to be allocated and touched
// repeatedly during each cycle collection.

struct PtrInfo
{
    void *mPointer;
    nsCycleCollectionParticipant *mParticipant;
    PRUint32 mColor : 2;
    PRUint32 mInternalRefs : 30;
    PRUint32 mRefCount;
private:
    EdgePool::Iterator mFirstChild;

public:
#ifdef DEBUG_CC
    size_t mBytes;
    char *mName;
    PRUint32 mLangID;

    // For finding roots in ExplainLiveExpectedGarbage (when there are
    // missing calls to suspect or failures to unlink).
    PRUint32 mSCCIndex; // strongly connected component

    // For finding roots in ExplainLiveExpectedGarbage (when nodes
    // expected to be garbage are black).
    ReversedEdge* mReversedEdges; // linked list
    PtrInfo* mShortestPathToExpectedGarbage;
    nsCString* mShortestPathToExpectedGarbageEdgeName;

    nsTArray<nsCString> mEdgeNames;
#endif

    PtrInfo(void *aPointer, nsCycleCollectionParticipant *aParticipant
            IF_DEBUG_CC_PARAM(PRUint32 aLangID)
            )
        : mPointer(aPointer),
          mParticipant(aParticipant),
          mColor(grey),
          mInternalRefs(0),
          mRefCount(0),
          mFirstChild()
#ifdef DEBUG_CC
        , mBytes(0),
          mName(nsnull),
          mLangID(aLangID),
          mSCCIndex(0),
          mReversedEdges(nsnull),
          mShortestPathToExpectedGarbage(nsnull),
          mShortestPathToExpectedGarbageEdgeName(nsnull)
#endif
    {
    }

#ifdef DEBUG_CC
    void Destroy() {
        PL_strfree(mName);
        mEdgeNames.~nsTArray<nsCString>();
    }
#endif

    // Allow NodePool::Block's constructor to compile.
    PtrInfo() {
        NS_NOTREACHED("should never be called");
    }

    EdgePool::Iterator FirstChild()
    {
        return mFirstChild;
    }

    // this PtrInfo must be part of a NodePool
    EdgePool::Iterator LastChild()
    {
        return (this + 1)->mFirstChild;
    }

    void SetFirstChild(EdgePool::Iterator aFirstChild)
    {
        mFirstChild = aFirstChild;
    }

    // this PtrInfo must be part of a NodePool
    void SetLastChild(EdgePool::Iterator aLastChild)
    {
        (this + 1)->mFirstChild = aLastChild;
    }
};

/**
 * A structure designed to be used like a linked list of PtrInfo, except
 * that allocates the PtrInfo 32K-at-a-time.
 */
class NodePool
{
private:
    enum { BlockSize = 8 * 1024 }; // could be int template parameter

    struct Block {
        // We create and destroy Block using NS_Alloc/NS_Free rather
        // than new and delete to avoid calling its constructor and
        // destructor.
        Block() { NS_NOTREACHED("should never be called"); }
        ~Block() { NS_NOTREACHED("should never be called"); }

        Block* mNext;
        PtrInfo mEntries[BlockSize + 1]; // +1 to store last child of last node
    };

public:
    NodePool()
        : mBlocks(nsnull),
          mLast(nsnull),
          mNumBlocks(0)
    {
    }

    ~NodePool()
    {
        NS_ASSERTION(!mBlocks, "Didn't call Clear()?");
    }

    void Clear()
    {
#ifdef DEBUG_CC
        {
            Enumerator queue(*this);
            while (!queue.IsDone()) {
                queue.GetNext()->Destroy();
            }
        }
#endif
        Block *b = mBlocks;
        while (b) {
            Block *n = b->mNext;
            NS_Free(b);
            NS_ASSERTION(mNumBlocks > 0,
                         "Expected NodePool mNumBlocks to be positive.");
            mNumBlocks--;
            b = n;
        }

        mBlocks = nsnull;
        mLast = nsnull;
    }

    class Builder;
    friend class Builder;
    class Builder {
    public:
        Builder(NodePool& aPool)
            : mNextBlock(&aPool.mBlocks),
              mNext(aPool.mLast),
              mBlockEnd(nsnull),
              mNumBlocks(aPool.mNumBlocks)
        {
            NS_ASSERTION(aPool.mBlocks == nsnull && aPool.mLast == nsnull,
                         "pool not empty");
        }
        PtrInfo *Add(void *aPointer, nsCycleCollectionParticipant *aParticipant
                     IF_DEBUG_CC_PARAM(PRUint32 aLangID)
                    )
        {
            if (mNext == mBlockEnd) {
                Block *block;
                if (!(*mNextBlock = block =
                        static_cast<Block*>(NS_Alloc(sizeof(Block)))))
                    return nsnull;
                mNext = block->mEntries;
                mBlockEnd = block->mEntries + BlockSize;
                block->mNext = nsnull;
                mNextBlock = &block->mNext;
                mNumBlocks++;
            }
            return new (mNext++) PtrInfo(aPointer, aParticipant
                                         IF_DEBUG_CC_PARAM(aLangID)
                                        );
        }
    private:
        Block **mNextBlock;
        PtrInfo *&mNext;
        PtrInfo *mBlockEnd;
        PRUint32 &mNumBlocks;
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

        bool IsDone() const
        {
            return mNext == mLast;
        }

        bool AtBlockEnd() const
        {
            return mNext == mBlockEnd;
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

    size_t BlocksSize() const {
        return sizeof(Block) * mNumBlocks;
    }

private:
    Block *mBlocks;
    PtrInfo *mLast;
    PRUint32 mNumBlocks;
};


struct WeakMapping
{
    // map and key will be null if the corresponding objects are GC marked
    PtrInfo *mMap;
    PtrInfo *mKey;
    PtrInfo *mVal;
};

class GCGraphBuilder;

struct GCGraph
{
    NodePool mNodes;
    EdgePool mEdges;
    nsTArray<WeakMapping> mWeakMaps;
    PRUint32 mRootCount;
#ifdef DEBUG_CC
    ReversedEdge *mReversedEdges;
#endif

    GCGraph() : mRootCount(0) {
    }
    ~GCGraph() { 
    }

    size_t BlocksSize() const {
        return mNodes.BlocksSize() + mEdges.BlocksSize();
    }

};

// XXX Would be nice to have an nsHashSet<KeyType> API that has
// Add/Remove/Has rather than PutEntry/RemoveEntry/GetEntry.
typedef nsTHashtable<nsVoidPtrHashKey> PointerSet;

static inline void
ToParticipant(nsISupports *s, nsXPCOMCycleCollectionParticipant **cp);

struct nsPurpleBuffer
{
private:
    struct Block {
        Block *mNext;
        nsPurpleBufferEntry mEntries[255];

        Block() : mNext(nsnull) {}
    };
public:
    // This class wraps a linked list of the elements in the purple
    // buffer.

    nsCycleCollectorParams &mParams;
    PRUint32 mNumBlocksAlloced;
    PRUint32 mCount;
    Block mFirstBlock;
    nsPurpleBufferEntry *mFreeList;

    // For objects compiled against Gecko 1.9 and 1.9.1.
    PointerSet mCompatObjects;
#ifdef DEBUG_CC
    PointerSet mNormalObjects; // duplicates our blocks
    nsCycleCollectorStats &mStats;
#endif
    
#ifdef DEBUG_CC
    nsPurpleBuffer(nsCycleCollectorParams &params,
                   nsCycleCollectorStats &stats) 
        : mParams(params),
          mStats(stats)
    {
        InitBlocks();
        mNormalObjects.Init();
        mCompatObjects.Init();
    }
#else
    nsPurpleBuffer(nsCycleCollectorParams &params) 
        : mParams(params)
    {
        InitBlocks();
        mCompatObjects.Init();
    }
#endif

    ~nsPurpleBuffer()
    {
        FreeBlocks();
    }

    void InitBlocks()
    {
        mNumBlocksAlloced = 0;
        mCount = 0;
        mFreeList = nsnull;
        StartBlock(&mFirstBlock);
    }

    void StartBlock(Block *aBlock)
    {
        NS_ABORT_IF_FALSE(!mFreeList, "should not have free list");

        // Put all the entries in the block on the free list.
        nsPurpleBufferEntry *entries = aBlock->mEntries;
        mFreeList = entries;
        for (PRUint32 i = 1; i < ArrayLength(aBlock->mEntries); ++i) {
            entries[i - 1].mNextInFreeList =
                (nsPurpleBufferEntry*)(PRUword(entries + i) | 1);
        }
        entries[ArrayLength(aBlock->mEntries) - 1].mNextInFreeList =
            (nsPurpleBufferEntry*)1;
    }

    void FreeBlocks()
    {
        if (mCount > 0)
            UnmarkRemainingPurple(&mFirstBlock);
        Block *b = mFirstBlock.mNext; 
        while (b) {
            if (mCount > 0)
                UnmarkRemainingPurple(b);
            Block *next = b->mNext;
            delete b;
            b = next;
            NS_ASSERTION(mNumBlocksAlloced > 0,
                         "Expected positive mNumBlocksAlloced.");
            mNumBlocksAlloced--;
        }
        mFirstBlock.mNext = nsnull;
    }

    void UnmarkRemainingPurple(Block *b)
    {
        for (nsPurpleBufferEntry *e = b->mEntries,
                              *eEnd = ArrayEnd(b->mEntries);
             e != eEnd; ++e) {
            if (!(PRUword(e->mObject) & PRUword(1))) {
                // This is a real entry (rather than something on the
                // free list).
                if (e->mObject) {
                    nsXPCOMCycleCollectionParticipant *cp;
                    ToParticipant(e->mObject, &cp);

                    cp->UnmarkPurple(e->mObject);
                }

                if (--mCount == 0)
                    break;
            }
        }
    }

    void SelectPointers(GCGraphBuilder &builder);

#ifdef DEBUG_CC
    void NoteAll(GCGraphBuilder &builder);

    bool Exists(void *p) const
    {
        return mNormalObjects.GetEntry(p) || mCompatObjects.GetEntry(p);
    }
#endif

    nsPurpleBufferEntry* NewEntry()
    {
        if (!mFreeList) {
            Block *b = new Block;
            if (!b) {
                return nsnull;
            }
            mNumBlocksAlloced++;
            StartBlock(b);

            // Add the new block as the second block in the list.
            b->mNext = mFirstBlock.mNext;
            mFirstBlock.mNext = b;
        }

        nsPurpleBufferEntry *e = mFreeList;
        mFreeList = (nsPurpleBufferEntry*)
            (PRUword(mFreeList->mNextInFreeList) & ~PRUword(1));
        return e;
    }

    nsPurpleBufferEntry* Put(nsISupports *p)
    {
        nsPurpleBufferEntry *e = NewEntry();
        if (!e) {
            return nsnull;
        }

        ++mCount;

        e->mObject = p;

#ifdef DEBUG_CC
        mNormalObjects.PutEntry(p);
#endif

        // Caller is responsible for filling in result's mRefCnt.
        return e;
    }

    void Remove(nsPurpleBufferEntry *e)
    {
        NS_ASSERTION(mCount != 0, "must have entries");

#ifdef DEBUG_CC
        mNormalObjects.RemoveEntry(e->mObject);
#endif

        e->mNextInFreeList =
            (nsPurpleBufferEntry*)(PRUword(mFreeList) | PRUword(1));
        mFreeList = e;

        --mCount;
    }

    bool PutCompatObject(nsISupports *p)
    {
        ++mCount;
        return !!mCompatObjects.PutEntry(p);
    }

    void RemoveCompatObject(nsISupports *p)
    {
        --mCount;
        mCompatObjects.RemoveEntry(p);
    }

    PRUint32 Count() const
    {
        return mCount;
    }

    size_t BlocksSize() const
    {
        return sizeof(Block) * mNumBlocksAlloced;
    }

};

struct CallbackClosure
{
    CallbackClosure(nsPurpleBuffer *aPurpleBuffer, GCGraphBuilder &aBuilder)
        : mPurpleBuffer(aPurpleBuffer),
          mBuilder(aBuilder)
    {
    }
    nsPurpleBuffer *mPurpleBuffer;
    GCGraphBuilder &mBuilder;
};

static bool
AddPurpleRoot(GCGraphBuilder &builder, nsISupports *root);

static PLDHashOperator
selectionCallback(nsVoidPtrHashKey* key, void* userArg)
{
    CallbackClosure *closure = static_cast<CallbackClosure*>(userArg);
    if (AddPurpleRoot(closure->mBuilder,
                      static_cast<nsISupports *>(
                        const_cast<void*>(key->GetKey()))))
        return PL_DHASH_REMOVE;

    return PL_DHASH_NEXT;
}

void
nsPurpleBuffer::SelectPointers(GCGraphBuilder &aBuilder)
{
#ifdef DEBUG_CC
    NS_ABORT_IF_FALSE(mCompatObjects.Count() + mNormalObjects.Count() ==
                          mCount,
                      "count out of sync");
#endif

    if (mCompatObjects.Count()) {
        mCount -= mCompatObjects.Count();
        CallbackClosure closure(this, aBuilder);
        mCompatObjects.EnumerateEntries(selectionCallback, &closure);
        mCount += mCompatObjects.Count(); // in case of allocation failure
    }

    // Walk through all the blocks.
    for (Block *b = &mFirstBlock; b; b = b->mNext) {
        for (nsPurpleBufferEntry *e = b->mEntries,
                              *eEnd = ArrayEnd(b->mEntries);
            e != eEnd; ++e) {
            if (!(PRUword(e->mObject) & PRUword(1))) {
                // This is a real entry (rather than something on the
                // free list).
                if (!e->mObject || AddPurpleRoot(aBuilder, e->mObject)) {
#ifdef DEBUG_CC
                    mNormalObjects.RemoveEntry(e->mObject);
#endif
                    --mCount;
                    // Put this entry on the free list in case some
                    // call to AddPurpleRoot fails and we don't rebuild
                    // the free list below.
                    e->mNextInFreeList = (nsPurpleBufferEntry*)
                        (PRUword(mFreeList) | PRUword(1));
                    mFreeList = e;
                }
            }
        }
    }

    NS_WARN_IF_FALSE(mCount == 0, "AddPurpleRoot failed");
    if (mCount == 0) {
        FreeBlocks();
        InitBlocks();
    }
}



////////////////////////////////////////////////////////////////////////
// Implement the LanguageRuntime interface for C++/XPCOM 
////////////////////////////////////////////////////////////////////////


struct nsCycleCollectionXPCOMRuntime : 
    public nsCycleCollectionLanguageRuntime 
{
    nsresult BeginCycleCollection(nsCycleCollectionTraversalCallback &cb,
                                  bool explainLiveExpectedGarbage)
    {
        return NS_OK;
    }

    nsresult FinishTraverse() 
    {
        return NS_OK;
    }

    nsresult FinishCycleCollection() 
    {
        return NS_OK;
    }

    inline nsCycleCollectionParticipant *ToParticipant(void *p);

#ifdef DEBUG_CC
    virtual void PrintAllReferencesTo(void *p) {}
#endif
};

struct nsCycleCollector
{
    bool mCollectionInProgress;
    bool mScanInProgress;
    bool mFollowupCollection;
    PRUint32 mCollectedObjects;
    TimeStamp mCollectionStart;

    nsCycleCollectionLanguageRuntime *mRuntimes[nsIProgrammingLanguage::MAX+1];
    nsCycleCollectionXPCOMRuntime mXPCOMRuntime;

    GCGraph mGraph;

    nsCycleCollectorParams mParams;

    nsTArray<PtrInfo*> *mWhiteNodes;
    PRUint32 mWhiteNodeCount;

    // mVisitedRefCounted and mVisitedGCed are only used for telemetry
    PRUint32 mVisitedRefCounted;
    PRUint32 mVisitedGCed;

    nsPurpleBuffer mPurpleBuf;

    void RegisterRuntime(PRUint32 langID, 
                         nsCycleCollectionLanguageRuntime *rt);
    nsCycleCollectionLanguageRuntime * GetRuntime(PRUint32 langID);
    void ForgetRuntime(PRUint32 langID);

    void SelectPurple(GCGraphBuilder &builder);
    void MarkRoots(GCGraphBuilder &builder);
    void ScanRoots();
    void ScanWeakMaps();

    // returns whether anything was collected
    bool CollectWhite(nsICycleCollectorListener *aListener);

    nsCycleCollector();
    ~nsCycleCollector();

    // The first pair of Suspect and Forget functions are only used by
    // old XPCOM binary components.
    bool Suspect(nsISupports *n);
    bool Forget(nsISupports *n);
    nsPurpleBufferEntry* Suspect2(nsISupports *n);
    bool Forget2(nsPurpleBufferEntry *e);

    PRUint32 Collect(PRUint32 aTryCollections,
                     nsICycleCollectorListener *aListener);

    // Prepare for and cleanup after one or more collection(s).
    bool PrepareForCollection(nsTArray<PtrInfo*> *aWhiteNodes);
    void GCIfNeeded(bool aForceGC);
    void CleanupAfterCollection();

    // Start and finish an individual collection.
    bool BeginCollection(nsICycleCollectorListener *aListener);
    bool FinishCollection(nsICycleCollectorListener *aListener);

    PRUint32 SuspectedCount();
    void Shutdown();

    void ClearGraph()
    {
        mGraph.mNodes.Clear();
        mGraph.mEdges.Clear();
        mGraph.mWeakMaps.Clear();
        mGraph.mRootCount = 0;
    }

#ifdef DEBUG_CC
    nsCycleCollectorStats mStats;

    FILE *mPtrLog;

    void Allocated(void *n, size_t sz);
    void Freed(void *n);

    void ExplainLiveExpectedGarbage();
    bool CreateReversedEdges();
    void DestroyReversedEdges();
    void ShouldBeFreed(nsISupports *n);
    void WasFreed(nsISupports *n);
    PointerSet mExpectedGarbage;
#endif
};


/**
 * GraphWalker is templatized over a Visitor class that must provide
 * the following two methods:
 *
 * bool ShouldVisitNode(PtrInfo const *pi);
 * void VisitNode(PtrInfo *pi);
 */
template <class Visitor>
class GraphWalker
{
private:
    Visitor mVisitor;

    void DoWalk(nsDeque &aQueue);

public:
    void Walk(PtrInfo *s0);
    void WalkFromRoots(GCGraph &aGraph);
    // copy-constructing the visitor should be cheap, and less
    // indirection than using a reference
    GraphWalker(const Visitor aVisitor) : mVisitor(aVisitor) {}
};


////////////////////////////////////////////////////////////////////////
// The static collector object
////////////////////////////////////////////////////////////////////////


static nsCycleCollector *sCollector = nsnull;


////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////

class CCRunnableFaultReport : public nsRunnable {
public:
    CCRunnableFaultReport(const nsCString& report)
    {
        CopyUTF8toUTF16(report, mReport);
    }
    
    NS_IMETHOD Run() {
        nsCOMPtr<nsIObserverService> obs =
            do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
        if (obs) {
            obs->NotifyObservers(nsnull, "cycle-collector-fault",
                                 mReport.get());
        }

        nsCOMPtr<nsIConsoleService> cons =
            do_GetService(NS_CONSOLESERVICE_CONTRACTID);
        if (cons) {
            cons->LogStringMessage(mReport.get());
        }
        return NS_OK;
    }

private:
    nsString mReport;
};

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

    nsPrintfCString str(256, "Fault in cycle collector: %s (ptr: %p)\n",
                        msg, ptr);
    NS_NOTREACHED(str.get());

    // When faults are not fatal, we assume we're running in a
    // production environment and we therefore want to disable the
    // collector on a fault. This will unfortunately cause the browser
    // to leak pretty fast wherever creates cyclical garbage, but it's
    // probably a better user experience than crashing. Besides, we
    // *should* never hit a fault.

    sCollector->mParams.mDoNothing = true;

    // Report to observers off an event so we don't run JS under GC
    // (which is where we might be right now).
    nsCOMPtr<nsIRunnable> ev = new CCRunnableFaultReport(str);
    NS_DispatchToMainThread(ev);
}

#ifdef DEBUG_CC
static void
Fault(const char *msg, PtrInfo *pi)
{
    printf("Fault in cycle collector: %s\n"
           "  while operating on pointer %p %s\n",
           msg, pi->mPointer, pi->mName);
    if (pi->mInternalRefs) {
        printf("  which has internal references from:\n");
        NodePool::Enumerator queue(sCollector->mGraph.mNodes);
        while (!queue.IsDone()) {
            PtrInfo *ppi = queue.GetNext();
            for (EdgePool::Iterator e = ppi->FirstChild(),
                                e_end = ppi->LastChild();
                 e != e_end; ++e) {
                if (*e == pi) {
                    printf("    %p %s\n", ppi->mPointer, ppi->mName);
                }
            }
        }
    }

    Fault(msg, pi->mPointer);
}
#else
inline void
Fault(const char *msg, PtrInfo *pi)
{
    Fault(msg, pi->mPointer);
}
#endif

static inline void
AbortIfOffMainThreadIfCheckFast()
{
#if defined(XP_WIN) || defined(NS_TLS)
    if (!NS_IsMainThread() && !NS_IsCycleCollectorThread()) {
        NS_RUNTIMEABORT("Main-thread-only object used off the main thread");
    }
#endif
}

static nsISupports *
canonicalize(nsISupports *in)
{
    nsISupports* child;
    in->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                       reinterpret_cast<void**>(&child));
    return child;
}

static inline void
ToParticipant(nsISupports *s, nsXPCOMCycleCollectionParticipant **cp)
{
    // We use QI to move from an nsISupports to an
    // nsXPCOMCycleCollectionParticipant, which is a per-class singleton helper
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

nsCycleCollectionParticipant *
nsCycleCollectionXPCOMRuntime::ToParticipant(void *p)
{
    nsXPCOMCycleCollectionParticipant *cp;
    ::ToParticipant(static_cast<nsISupports*>(p), &cp);
    return cp;
}


template <class Visitor>
void
GraphWalker<Visitor>::Walk(PtrInfo *s0)
{
    nsDeque queue;
    queue.Push(s0);
    DoWalk(queue);
}

template <class Visitor>
void
GraphWalker<Visitor>::WalkFromRoots(GCGraph& aGraph)
{
    nsDeque queue;
    NodePool::Enumerator etor(aGraph.mNodes);
    for (PRUint32 i = 0; i < aGraph.mRootCount; ++i) {
        queue.Push(etor.GetNext());
    }
    DoWalk(queue);
}

template <class Visitor>
void
GraphWalker<Visitor>::DoWalk(nsDeque &aQueue)
{
    // Use a aQueue to match the breadth-first traversal used when we
    // built the graph, for hopefully-better locality.
    while (aQueue.GetSize() > 0) {
        PtrInfo *pi = static_cast<PtrInfo*>(aQueue.PopFront());

        if (mVisitor.ShouldVisitNode(pi)) {
            mVisitor.VisitNode(pi);
            for (EdgePool::Iterator child = pi->FirstChild(),
                                child_end = pi->LastChild();
                 child != child_end; ++child) {
                aQueue.Push(*child);
            }
        }
    };

#ifdef DEBUG_CC
    sCollector->mStats.mWalkedGraph++;
#endif
}


class nsCycleCollectorLogger : public nsICycleCollectorListener
{
public:
    nsCycleCollectorLogger() : mStream(nsnull)
    {
    }
    ~nsCycleCollectorLogger()
    {
        if (mStream) {
            fclose(mStream);
        }
    }
    NS_DECL_ISUPPORTS

    NS_IMETHOD Begin()
    {
        char name[255];
        sprintf(name, "cc-edges-%d.%d.log", ++gLogCounter, base::GetCurrentProcId());
        mStream = fopen(name, "w");

        return mStream ? NS_OK : NS_ERROR_FAILURE;
    }
    NS_IMETHOD NoteRefCountedObject(PRUint64 aAddress, PRUint32 refCount,
                                    const char *aObjectDescription)
    {
        fprintf(mStream, "%p [rc=%u] %s\n", (void*)aAddress, refCount,
                aObjectDescription);

        return NS_OK;
    }
    NS_IMETHOD NoteGCedObject(PRUint64 aAddress, bool aMarked,
                              const char *aObjectDescription)
    {
        fprintf(mStream, "%p [gc%s] %s\n", (void*)aAddress,
                aMarked ? ".marked" : "", aObjectDescription);

        return NS_OK;
    }
    NS_IMETHOD NoteEdge(PRUint64 aToAddress, const char *aEdgeName)
    {
        fprintf(mStream, "> %p %s\n", (void*)aToAddress, aEdgeName);

        return NS_OK;
    }
    NS_IMETHOD BeginResults()
    {
        fputs("==========\n", mStream);

        return NS_OK;
    }
    NS_IMETHOD DescribeRoot(PRUint64 aAddress, PRUint32 aKnownEdges)
    {
        fprintf(mStream, "%p [known=%u]\n", (void*)aAddress, aKnownEdges);

        return NS_OK;
    }
    NS_IMETHOD DescribeGarbage(PRUint64 aAddress)
    {
        fprintf(mStream, "%p [garbage]\n", (void*)aAddress);

        return NS_OK;
    }
    NS_IMETHOD End()
    {
        fclose(mStream);
        mStream = nsnull;

        return NS_OK;
    }

private:
    FILE *mStream;

    static PRUint32 gLogCounter;
};

NS_IMPL_ISUPPORTS1(nsCycleCollectorLogger, nsICycleCollectorListener)

PRUint32 nsCycleCollectorLogger::gLogCounter = 0;

nsresult
nsCycleCollectorLoggerConstructor(nsISupports* aOuter,
                                  const nsIID& aIID,
                                  void* *aInstancePtr)
{
    NS_ENSURE_TRUE(!aOuter, NS_ERROR_NO_AGGREGATION);

    nsISupports *logger = new nsCycleCollectorLogger();

    return logger->QueryInterface(aIID, aInstancePtr);
}

////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |MarkRoots| routine.
////////////////////////////////////////////////////////////////////////

struct PtrToNodeEntry : public PLDHashEntryHdr
{
    // The key is mNode->mPointer
    PtrInfo *mNode;
};

static bool
PtrToNodeMatchEntry(PLDHashTable *table,
                    const PLDHashEntryHdr *entry,
                    const void *key)
{
    const PtrToNodeEntry *n = static_cast<const PtrToNodeEntry*>(entry);
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

class GCGraphBuilder : public nsCycleCollectionTraversalCallback
{
private:
    NodePool::Builder mNodeBuilder;
    EdgePool::Builder mEdgeBuilder;
    nsTArray<WeakMapping> &mWeakMaps;
    PLDHashTable mPtrToNodeMap;
    PtrInfo *mCurrPi;
    nsCycleCollectionLanguageRuntime **mRuntimes; // weak, from nsCycleCollector
    nsCString mNextEdgeName;
    nsICycleCollectorListener *mListener;

public:
    GCGraphBuilder(GCGraph &aGraph,
                   nsCycleCollectionLanguageRuntime **aRuntimes,
                   nsICycleCollectorListener *aListener);
    ~GCGraphBuilder();
    bool Initialized();

    PRUint32 Count() const { return mPtrToNodeMap.entryCount; }

#ifdef DEBUG_CC
    PtrInfo* AddNode(void *s, nsCycleCollectionParticipant *aParticipant,
                     PRUint32 aLangID);
#else
    PtrInfo* AddNode(void *s, nsCycleCollectionParticipant *aParticipant);
    PtrInfo* AddNode(void *s, nsCycleCollectionParticipant *aParticipant,
                     PRUint32 aLangID)
    {
        return AddNode(s, aParticipant);
    }
#endif
    PtrInfo* AddWeakMapNode(void* node);
    void Traverse(PtrInfo* aPtrInfo);
    void SetLastChild();

    // nsCycleCollectionTraversalCallback methods.
    NS_IMETHOD_(void) NoteXPCOMRoot(nsISupports *root);

private:
    void DescribeNode(PRUint32 refCount,
                      size_t objSz,
                      const char *objName)
    {
        mCurrPi->mRefCount = refCount;
#ifdef DEBUG_CC
        mCurrPi->mBytes = objSz;
        mCurrPi->mName = PL_strdup(objName);
        sCollector->mStats.mVisitedNode++;
#endif
    }

    NS_IMETHOD_(void) DescribeRefCountedNode(nsrefcnt refCount, size_t objSz,
                                             const char *objName);
    NS_IMETHOD_(void) DescribeGCedNode(bool isMarked, size_t objSz,
                                       const char *objName);
    NS_IMETHOD_(void) NoteRoot(PRUint32 langID, void *child,
                               nsCycleCollectionParticipant* participant);
    NS_IMETHOD_(void) NoteXPCOMChild(nsISupports *child);
    NS_IMETHOD_(void) NoteNativeChild(void *child,
                                     nsCycleCollectionParticipant *participant);
    NS_IMETHOD_(void) NoteScriptChild(PRUint32 langID, void *child);
    NS_IMETHOD_(void) NoteNextEdgeName(const char* name);
    NS_IMETHOD_(void) NoteWeakMapping(void *map, void *key, void *val);
};

GCGraphBuilder::GCGraphBuilder(GCGraph &aGraph,
                               nsCycleCollectionLanguageRuntime **aRuntimes,
                               nsICycleCollectorListener *aListener)
    : mNodeBuilder(aGraph.mNodes),
      mEdgeBuilder(aGraph.mEdges),
      mWeakMaps(aGraph.mWeakMaps),
      mRuntimes(aRuntimes),
      mListener(aListener)
{
    if (!PL_DHashTableInit(&mPtrToNodeMap, &PtrNodeOps, nsnull,
                           sizeof(PtrToNodeEntry), 32768))
        mPtrToNodeMap.ops = nsnull;
    // We want all edges and all info if DEBUG_CC is set or if we have a
    // listener. Do we want them all the time?
#ifndef DEBUG_CC
    if (mListener)
#endif
    {
        mFlags |= nsCycleCollectionTraversalCallback::WANT_DEBUG_INFO |
                  nsCycleCollectionTraversalCallback::WANT_ALL_TRACES;
    }
}

GCGraphBuilder::~GCGraphBuilder()
{
    if (mPtrToNodeMap.ops)
        PL_DHashTableFinish(&mPtrToNodeMap);
}

bool
GCGraphBuilder::Initialized()
{
    return !!mPtrToNodeMap.ops;
}

PtrInfo*
GCGraphBuilder::AddNode(void *s, nsCycleCollectionParticipant *aParticipant
                        IF_DEBUG_CC_PARAM(PRUint32 aLangID)
                       )
{
    PtrToNodeEntry *e = static_cast<PtrToNodeEntry*>(PL_DHashTableOperate(&mPtrToNodeMap, s, PL_DHASH_ADD));
    if (!e)
        return nsnull;

    PtrInfo *result;
    if (!e->mNode) {
        // New entry.
        result = mNodeBuilder.Add(s, aParticipant
                                  IF_DEBUG_CC_PARAM(aLangID)
                                 );
        if (!result) {
            PL_DHashTableRawRemove(&mPtrToNodeMap, e);
            return nsnull;
        }
        e->mNode = result;
    } else {
        result = e->mNode;
        NS_ASSERTION(result->mParticipant == aParticipant,
                     "nsCycleCollectionParticipant shouldn't change!");
    }
    return result;
}

void
GCGraphBuilder::Traverse(PtrInfo* aPtrInfo)
{
    mCurrPi = aPtrInfo;

#ifdef DEBUG_CC
    if (!mCurrPi->mParticipant) {
        Fault("unknown pointer during walk", aPtrInfo);
        return;
    }
#endif

    mCurrPi->SetFirstChild(mEdgeBuilder.Mark());

    nsresult rv = aPtrInfo->mParticipant->Traverse(aPtrInfo->mPointer, *this);
    if (NS_FAILED(rv)) {
        Fault("script pointer traversal failed", aPtrInfo);
    }
}

void
GCGraphBuilder::SetLastChild()
{
    mCurrPi->SetLastChild(mEdgeBuilder.Mark());
}

NS_IMETHODIMP_(void)
GCGraphBuilder::NoteXPCOMRoot(nsISupports *root)
{
    root = canonicalize(root);
    NS_ASSERTION(root,
                 "Don't add objects that don't participate in collection!");

#ifdef DEBUG_CC
    if (nsCycleCollector_shouldSuppress(root))
        return;
#endif
    
    nsXPCOMCycleCollectionParticipant *cp;
    ToParticipant(root, &cp);

    NoteRoot(nsIProgrammingLanguage::CPLUSPLUS, root, cp);
}


NS_IMETHODIMP_(void)
GCGraphBuilder::NoteRoot(PRUint32 langID, void *root,
                         nsCycleCollectionParticipant* participant)
{
    NS_ASSERTION(root, "Don't add a null root!");

    if (langID > nsIProgrammingLanguage::MAX || !mRuntimes[langID]) {
        Fault("adding root for unregistered language", root);
        return;
    }

    AddNode(root, participant, langID);
}

NS_IMETHODIMP_(void)
GCGraphBuilder::DescribeRefCountedNode(nsrefcnt refCount, size_t objSz,
                                       const char *objName)
{
    if (refCount == 0)
        Fault("zero refcount", mCurrPi);
    if (refCount == PR_UINT32_MAX)
        Fault("overflowing refcount", mCurrPi);
    sCollector->mVisitedRefCounted++;

    if (mListener) {
        mListener->NoteRefCountedObject((PRUint64)mCurrPi->mPointer, refCount,
                                        objName);
    }

    DescribeNode(refCount, objSz, objName);
}

NS_IMETHODIMP_(void)
GCGraphBuilder::DescribeGCedNode(bool isMarked, size_t objSz,
                                 const char *objName)
{
    PRUint32 refCount = isMarked ? PR_UINT32_MAX : 0;
    sCollector->mVisitedGCed++;

    if (mListener) {
        mListener->NoteGCedObject((PRUint64)mCurrPi->mPointer, isMarked,
                                  objName);
    }

    DescribeNode(refCount, objSz, objName);
}

NS_IMETHODIMP_(void)
GCGraphBuilder::NoteXPCOMChild(nsISupports *child) 
{
    nsCString edgeName;
    if (WantDebugInfo()) {
        edgeName.Assign(mNextEdgeName);
        mNextEdgeName.Truncate();
    }
    if (!child || !(child = canonicalize(child)))
        return; 

#ifdef DEBUG_CC
    if (nsCycleCollector_shouldSuppress(child))
        return;
#endif
    
    nsXPCOMCycleCollectionParticipant *cp;
    ToParticipant(child, &cp);
    if (cp) {
        PtrInfo *childPi = AddNode(child, cp, nsIProgrammingLanguage::CPLUSPLUS);
        if (!childPi)
            return;
        mEdgeBuilder.Add(childPi);
#ifdef DEBUG_CC
        mCurrPi->mEdgeNames.AppendElement(edgeName);
#endif
        if (mListener) {
            mListener->NoteEdge((PRUint64)child, edgeName.get());
        }
        ++childPi->mInternalRefs;
    }
}

NS_IMETHODIMP_(void)
GCGraphBuilder::NoteNativeChild(void *child,
                                nsCycleCollectionParticipant *participant)
{
    nsCString edgeName;
    if (WantDebugInfo()) {
        edgeName.Assign(mNextEdgeName);
        mNextEdgeName.Truncate();
    }
    if (!child)
        return;

    NS_ASSERTION(participant, "Need a nsCycleCollectionParticipant!");

    PtrInfo *childPi = AddNode(child, participant, nsIProgrammingLanguage::CPLUSPLUS);
    if (!childPi)
        return;
    mEdgeBuilder.Add(childPi);
#ifdef DEBUG_CC
    mCurrPi->mEdgeNames.AppendElement(edgeName);
#endif
    if (mListener) {
        mListener->NoteEdge((PRUint64)child, edgeName.get());
    }
    ++childPi->mInternalRefs;
}

NS_IMETHODIMP_(void)
GCGraphBuilder::NoteScriptChild(PRUint32 langID, void *child) 
{
    nsCString edgeName;
    if (WantDebugInfo()) {
        edgeName.Assign(mNextEdgeName);
        mNextEdgeName.Truncate();
    }
    if (!child)
        return;

    if (langID > nsIProgrammingLanguage::MAX) {
        Fault("traversing pointer for unknown language", child);
        return;
    }

    if (!mRuntimes[langID]) {
        NS_WARNING("Not collecting cycles involving objects for scripting "
                   "languages that don't participate in cycle collection.");
        return;
    }

    // skip over non-grey JS children
    if (langID == nsIProgrammingLanguage::JAVASCRIPT &&
        !xpc_GCThingIsGrayCCThing(child) && !WantAllTraces()) {
        return;
    }

    nsCycleCollectionParticipant *cp = mRuntimes[langID]->ToParticipant(child);
    if (!cp)
        return;

    PtrInfo *childPi = AddNode(child, cp, langID);
    if (!childPi)
        return;
    mEdgeBuilder.Add(childPi);
#ifdef DEBUG_CC
    mCurrPi->mEdgeNames.AppendElement(edgeName);
#endif
    if (mListener) {
        mListener->NoteEdge((PRUint64)child, edgeName.get());
    }
    ++childPi->mInternalRefs;
}

NS_IMETHODIMP_(void)
GCGraphBuilder::NoteNextEdgeName(const char* name)
{
    if (WantDebugInfo()) {
        mNextEdgeName = name;
    }
}

PtrInfo*
GCGraphBuilder::AddWeakMapNode(void *node)
{
    nsCycleCollectionParticipant *cp;
    NS_ASSERTION(node, "Weak map node should be non-null.");

    if (!xpc_GCThingIsGrayCCThing(node) && !WantAllTraces())
        return nsnull;

    cp = mRuntimes[nsIProgrammingLanguage::JAVASCRIPT]->ToParticipant(node);
    NS_ASSERTION(cp, "Javascript runtime participant should be non-null.");
    return AddNode(node, cp);
}

NS_IMETHODIMP_(void)
GCGraphBuilder::NoteWeakMapping(void *map, void *key, void *val)
{
    PtrInfo *valNode = AddWeakMapNode(val);

    if (!valNode)
        return;

    WeakMapping *mapping = mWeakMaps.AppendElement();
    mapping->mMap = map ? AddWeakMapNode(map) : nsnull;
    mapping->mKey = key ? AddWeakMapNode(key) : nsnull;
    mapping->mVal = valNode;
}

static bool
AddPurpleRoot(GCGraphBuilder &builder, nsISupports *root)
{
    root = canonicalize(root);
    NS_ASSERTION(root,
                 "Don't add objects that don't participate in collection!");

    nsXPCOMCycleCollectionParticipant *cp;
    ToParticipant(root, &cp);

    PtrInfo *pinfo = builder.AddNode(root, cp,
                                     nsIProgrammingLanguage::CPLUSPLUS);
    if (!pinfo) {
        return false;
    }

    cp->UnmarkPurple(root);

    return true;
}

#ifdef DEBUG_CC
static PLDHashOperator
noteAllCallback(nsVoidPtrHashKey* key, void* userArg)
{
    GCGraphBuilder *builder = static_cast<GCGraphBuilder*>(userArg);
    builder->NoteXPCOMRoot(
      static_cast<nsISupports *>(const_cast<void*>(key->GetKey())));
    return PL_DHASH_NEXT;
}

void
nsPurpleBuffer::NoteAll(GCGraphBuilder &builder)
{
    mCompatObjects.EnumerateEntries(noteAllCallback, &builder);

    for (Block *b = &mFirstBlock; b; b = b->mNext) {
        for (nsPurpleBufferEntry *e = b->mEntries,
                              *eEnd = ArrayEnd(b->mEntries);
            e != eEnd; ++e) {
            if (!(PRUword(e->mObject) & PRUword(1)) && e->mObject) {
                builder.NoteXPCOMRoot(e->mObject);
            }
        }
    }
}
#endif

void 
nsCycleCollector::SelectPurple(GCGraphBuilder &builder)
{
    mPurpleBuf.SelectPointers(builder);
}

void
nsCycleCollector::MarkRoots(GCGraphBuilder &builder)
{
    mGraph.mRootCount = builder.Count();

    // read the PtrInfo out of the graph that we are building
    NodePool::Enumerator queue(mGraph.mNodes);
    while (!queue.IsDone()) {
        PtrInfo *pi = queue.GetNext();
        builder.Traverse(pi);
        if (queue.AtBlockEnd())
            builder.SetLastChild();
    }
    if (mGraph.mRootCount > 0)
        builder.SetLastChild();
}


////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |ScanRoots| routine.
////////////////////////////////////////////////////////////////////////


struct ScanBlackVisitor
{
    ScanBlackVisitor(PRUint32 &aWhiteNodeCount)
        : mWhiteNodeCount(aWhiteNodeCount)
    {
    }

    bool ShouldVisitNode(PtrInfo const *pi)
    { 
        return pi->mColor != black;
    }

    void VisitNode(PtrInfo *pi)
    {
        if (pi->mColor == white)
            --mWhiteNodeCount;
        pi->mColor = black;
#ifdef DEBUG_CC
        sCollector->mStats.mSetColorBlack++;
#endif
    }

    PRUint32 &mWhiteNodeCount;
};


struct scanVisitor
{
    scanVisitor(PRUint32 &aWhiteNodeCount) : mWhiteNodeCount(aWhiteNodeCount)
    {
    }

    bool ShouldVisitNode(PtrInfo const *pi)
    { 
        return pi->mColor == grey;
    }

    void VisitNode(PtrInfo *pi)
    {
        if (pi->mInternalRefs > pi->mRefCount && pi->mRefCount > 0)
            Fault("traversed refs exceed refcount", pi);

        if (pi->mInternalRefs == pi->mRefCount || pi->mRefCount == 0) {
            pi->mColor = white;
            ++mWhiteNodeCount;
#ifdef DEBUG_CC
            sCollector->mStats.mSetColorWhite++;
#endif
        } else {
            GraphWalker<ScanBlackVisitor>(ScanBlackVisitor(mWhiteNodeCount)).Walk(pi);
            NS_ASSERTION(pi->mColor == black,
                         "Why didn't ScanBlackVisitor make pi black?");
        }
    }

    PRUint32 &mWhiteNodeCount;
};

// Iterate over the WeakMaps.  If we mark anything while iterating
// over the WeakMaps, we must iterate over all of the WeakMaps again.
void
nsCycleCollector::ScanWeakMaps()
{
    bool anyChanged;
    do {
        anyChanged = false;
        for (PRUint32 i = 0; i < mGraph.mWeakMaps.Length(); i++) {
            WeakMapping *wm = &mGraph.mWeakMaps[i];

            // If mMap or mKey are null, the original object was marked black.
            uint32 mColor = wm->mMap ? wm->mMap->mColor : black;
            uint32 kColor = wm->mKey ? wm->mKey->mColor : black;
            PtrInfo *v = wm->mVal;

            // All non-null weak mapping maps, keys and values are
            // roots (in the sense of WalkFromRoots) in the cycle
            // collector graph, and thus should have been colored
            // either black or white in ScanRoots().
            NS_ASSERTION(mColor != grey, "Uncolored weak map");
            NS_ASSERTION(kColor != grey, "Uncolored weak map key");
            NS_ASSERTION(v->mColor != grey, "Uncolored weak map value");

            if (mColor == black && kColor == black && v->mColor != black) {
                GraphWalker<ScanBlackVisitor>(ScanBlackVisitor(mWhiteNodeCount)).Walk(v);
                anyChanged = true;
            }
        }
    } while (anyChanged);
}

void
nsCycleCollector::ScanRoots()
{
    mWhiteNodeCount = 0;

    // On the assumption that most nodes will be black, it's
    // probably faster to use a GraphWalker than a
    // NodePool::Enumerator.
    GraphWalker<scanVisitor>(scanVisitor(mWhiteNodeCount)).WalkFromRoots(mGraph); 

    ScanWeakMaps();

#ifdef DEBUG_CC
    // Sanity check: scan should have colored all grey nodes black or
    // white. So we ensure we have no grey nodes at this point.
    NodePool::Enumerator etor(mGraph.mNodes);
    while (!etor.IsDone())
    {
        PtrInfo *pinfo = etor.GetNext();
        if (pinfo->mColor == grey) {
            Fault("valid grey node after scanning", pinfo);
        }
    }
#endif
}


////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |CollectWhite| routine, somewhat modified.
////////////////////////////////////////////////////////////////////////

bool
nsCycleCollector::CollectWhite(nsICycleCollectorListener *aListener)
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

    nsresult rv;

    NS_ASSERTION(mWhiteNodes->IsEmpty(),
                 "FinishCollection wasn't called?");

    mWhiteNodes->SetCapacity(mWhiteNodeCount);

    NodePool::Enumerator etor(mGraph.mNodes);
    while (!etor.IsDone())
    {
        PtrInfo *pinfo = etor.GetNext();
        if (pinfo->mColor == white && mWhiteNodes->AppendElement(pinfo)) {
            rv = pinfo->mParticipant->Root(pinfo->mPointer);
            if (NS_FAILED(rv)) {
                Fault("Failed root call while unlinking", pinfo);
                mWhiteNodes->RemoveElementAt(mWhiteNodes->Length() - 1);
            }
        }
    }

#if defined(DEBUG_CC) && !defined(__MINGW32__) && defined(WIN32)
    struct _CrtMemState ms1, ms2;
    _CrtMemCheckpoint(&ms1);
#endif

    PRUint32 i, count = mWhiteNodes->Length();

    if (aListener) {
        for (i = 0; i < count; ++i) {
            PtrInfo *pinfo = mWhiteNodes->ElementAt(i);
            aListener->DescribeGarbage((PRUint64)pinfo->mPointer);
        }
        aListener->End();
    }

    for (i = 0; i < count; ++i) {
        PtrInfo *pinfo = mWhiteNodes->ElementAt(i);
        rv = pinfo->mParticipant->Unlink(pinfo->mPointer);
        if (NS_FAILED(rv)) {
            Fault("Failed unlink call while unlinking", pinfo);
#ifdef DEBUG_CC
            mStats.mFailedUnlink++;
#endif
        }
        else {
#ifdef DEBUG_CC
            ++mStats.mCollectedNode;
#endif
        }
    }

    for (i = 0; i < count; ++i) {
        PtrInfo *pinfo = mWhiteNodes->ElementAt(i);
        rv = pinfo->mParticipant->Unroot(pinfo->mPointer);
        if (NS_FAILED(rv))
            Fault("Failed unroot call while unlinking", pinfo);
    }

#if defined(DEBUG_CC) && !defined(__MINGW32__) && defined(WIN32)
    _CrtMemCheckpoint(&ms2);
    if (ms2.lTotalCount < ms1.lTotalCount)
        mStats.mFreedBytes += (ms1.lTotalCount - ms2.lTotalCount);
#endif

    mCollectedObjects += count;
    return count > 0;
}


#ifdef DEBUG_CC
////////////////////////////////////////////////////////////////////////
// Memory-hooking stuff
// When debugging wild pointers, it sometimes helps to hook malloc and
// free. This stuff is disabled unless you set an environment variable.
////////////////////////////////////////////////////////////////////////

static bool hookedMalloc = false;

#if defined(__GLIBC__) && !defined(__UCLIBC__)
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
        hookedMalloc = true;        
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
        hookedMalloc = true;        
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
        hookedMalloc = true;
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
    mCollectionInProgress(false),
    mScanInProgress(false),
    mCollectedObjects(0),
    mWhiteNodes(nsnull),
    mWhiteNodeCount(0),
    mVisitedRefCounted(0),
    mVisitedGCed(0),
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

nsCycleCollectionLanguageRuntime *
nsCycleCollector::GetRuntime(PRUint32 langID)
{
    if (langID > nsIProgrammingLanguage::MAX)
        return nsnull;

    return mRuntimes[langID];
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

class Suppressor :
    public nsCycleCollectionTraversalCallback
{
protected:
    static char *sSuppressionList;
    static bool sInitialized;
    bool mSuppressThisNode;
public:
    Suppressor()
    {
    }

    bool shouldSuppress(nsISupports *s)
    {
        if (!sInitialized) {
            sSuppressionList = PR_GetEnv("XPCOM_CC_SUPPRESS");
            sInitialized = true;
        }
        if (sSuppressionList == nsnull) {
            mSuppressThisNode = false;
        } else {
            nsresult rv;
            nsXPCOMCycleCollectionParticipant *cp;
            rv = CallQueryInterface(s, &cp);
            if (NS_FAILED(rv)) {
                Fault("checking suppression on wrong type of pointer", s);
                return true;
            }
            cp->Traverse(s, *this);
        }
        return mSuppressThisNode;
    }

    NS_IMETHOD_(void) DescribeRefCountedNode(nsrefcnt refCount, size_t objSz,
                                             const char *objName)
    {
        mSuppressThisNode = (PL_strstr(sSuppressionList, objName) != nsnull);
    }

    NS_IMETHOD_(void) DescribeGCedNode(bool isMarked, size_t objSz,
                                       const char *objName)
    {
        mSuppressThisNode = (PL_strstr(sSuppressionList, objName) != nsnull);
    }

    NS_IMETHOD_(void) NoteXPCOMRoot(nsISupports *root) {};
    NS_IMETHOD_(void) NoteRoot(PRUint32 langID, void *root,
                               nsCycleCollectionParticipant* participant) {};
    NS_IMETHOD_(void) NoteXPCOMChild(nsISupports *child) {}
    NS_IMETHOD_(void) NoteScriptChild(PRUint32 langID, void *child) {}
    NS_IMETHOD_(void) NoteNativeChild(void *child,
                                     nsCycleCollectionParticipant *participant) {}
    NS_IMETHOD_(void) NoteNextEdgeName(const char* name) {}
    NS_IMETHOD_(void) NoteWeakMapping(void *map, void *key, void *val) {}
};

char *Suppressor::sSuppressionList = nsnull;
bool Suppressor::sInitialized = false;

static bool
nsCycleCollector_shouldSuppress(nsISupports *s)
{
    Suppressor supp;
    return supp.shouldSuppress(s);
}
#endif

#ifdef DEBUG
static bool
nsCycleCollector_isScanSafe(nsISupports *s)
{
    if (!s)
        return false;

    nsXPCOMCycleCollectionParticipant *cp;
    ToParticipant(s, &cp);

    return cp != nsnull;
}
#endif

bool
nsCycleCollector::Suspect(nsISupports *n)
{
    AbortIfOffMainThreadIfCheckFast();

    // Re-entering ::Suspect during collection used to be a fault, but
    // we are canonicalizing nsISupports pointers using QI, so we will
    // see some spurious refcount traffic here. 

    if (mScanInProgress)
        return false;

    NS_ASSERTION(nsCycleCollector_isScanSafe(n),
                 "suspected a non-scansafe pointer");

    if (mParams.mDoNothing)
        return false;

#ifdef DEBUG_CC
    mStats.mSuspectNode++;

    if (nsCycleCollector_shouldSuppress(n))
        return false;

#ifndef __MINGW32__
    if (mParams.mHookMalloc)
        InitMemHook();
#endif

    if (mParams.mLogPointers) {
        if (!mPtrLog)
            mPtrLog = fopen("pointer_log", "w");
        fprintf(mPtrLog, "S %p\n", static_cast<void*>(n));
    }
#endif

    return mPurpleBuf.PutCompatObject(n);
}


bool
nsCycleCollector::Forget(nsISupports *n)
{
    AbortIfOffMainThreadIfCheckFast();

    // Re-entering ::Forget during collection used to be a fault, but
    // we are canonicalizing nsISupports pointers using QI, so we will
    // see some spurious refcount traffic here. 

    if (mScanInProgress)
        return false;

    if (mParams.mDoNothing)
        return true; // it's as good as forgotten

#ifdef DEBUG_CC
    mStats.mForgetNode++;

#ifndef __MINGW32__
    if (mParams.mHookMalloc)
        InitMemHook();
#endif

    if (mParams.mLogPointers) {
        if (!mPtrLog)
            mPtrLog = fopen("pointer_log", "w");
        fprintf(mPtrLog, "F %p\n", static_cast<void*>(n));
    }
#endif

    mPurpleBuf.RemoveCompatObject(n);
    return true;
}

nsPurpleBufferEntry*
nsCycleCollector::Suspect2(nsISupports *n)
{
    AbortIfOffMainThreadIfCheckFast();

    // Re-entering ::Suspect during collection used to be a fault, but
    // we are canonicalizing nsISupports pointers using QI, so we will
    // see some spurious refcount traffic here. 

    if (mScanInProgress)
        return nsnull;

    NS_ASSERTION(nsCycleCollector_isScanSafe(n),
                 "suspected a non-scansafe pointer");

    if (mParams.mDoNothing)
        return nsnull;

#ifdef DEBUG_CC
    mStats.mSuspectNode++;

    if (nsCycleCollector_shouldSuppress(n))
        return nsnull;

#ifndef __MINGW32__
    if (mParams.mHookMalloc)
        InitMemHook();
#endif

    if (mParams.mLogPointers) {
        if (!mPtrLog)
            mPtrLog = fopen("pointer_log", "w");
        fprintf(mPtrLog, "S %p\n", static_cast<void*>(n));
    }
#endif

    // Caller is responsible for filling in result's mRefCnt.
    return mPurpleBuf.Put(n);
}


bool
nsCycleCollector::Forget2(nsPurpleBufferEntry *e)
{
    AbortIfOffMainThreadIfCheckFast();

    // Re-entering ::Forget during collection used to be a fault, but
    // we are canonicalizing nsISupports pointers using QI, so we will
    // see some spurious refcount traffic here. 

    if (mScanInProgress)
        return false;

#ifdef DEBUG_CC
    mStats.mForgetNode++;

#ifndef __MINGW32__
    if (mParams.mHookMalloc)
        InitMemHook();
#endif

    if (mParams.mLogPointers) {
        if (!mPtrLog)
            mPtrLog = fopen("pointer_log", "w");
        fprintf(mPtrLog, "F %p\n", static_cast<void*>(e->mObject));
    }
#endif

    mPurpleBuf.Remove(e);
    return true;
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
        
        if (mParams.mLogPointers) {
            if (!mPtrLog)
                mPtrLog = fopen("pointer_log", "w");
            fprintf(mPtrLog, "R %p\n", n);
        }
    }
}
#endif

// The cycle collector uses the mark bitmap to discover what JS objects
// were reachable only from XPConnect roots that might participate in
// cycles. We ask the JS runtime whether we need to force a GC before
// this CC. It returns true on startup (before the mark bits have been set),
// and also when UnmarkGray has run out of stack.  We also force GCs on shut 
// down to collect cycles involving both DOM and JS.
void
nsCycleCollector::GCIfNeeded(bool aForceGC)
{
    NS_ASSERTION(NS_IsMainThread(),
                 "nsCycleCollector::GCIfNeeded() must be called on the main thread.");

    if (mParams.mDoNothing)
        return;

    if (!mRuntimes[nsIProgrammingLanguage::JAVASCRIPT])
        return;

    nsCycleCollectionJSRuntime* rt =
        static_cast<nsCycleCollectionJSRuntime*>
            (mRuntimes[nsIProgrammingLanguage::JAVASCRIPT]);
    if (!rt->NeedCollect() && !aForceGC)
        return;

#ifdef COLLECT_TIME_DEBUG
    PRTime start = PR_Now();
#endif
    // rt->Collect() must be called from the main thread,
    // because it invokes XPCJSRuntime::GCCallback(cx, JSGC_BEGIN)
    // which returns false if not in the main thread.
    rt->Collect();
#ifdef COLLECT_TIME_DEBUG
    printf("cc: GC() took %lldms\n", (PR_Now() - start) / PR_USEC_PER_MSEC);
#endif
}

bool
nsCycleCollector::PrepareForCollection(nsTArray<PtrInfo*> *aWhiteNodes)
{
#if defined(DEBUG_CC) && !defined(__MINGW32__)
    if (!mParams.mDoNothing && mParams.mHookMalloc)
        InitMemHook();
#endif

    // This can legitimately happen in a few cases. See bug 383651.
    if (mCollectionInProgress)
        return false;

    NS_TIME_FUNCTION;

#ifdef COLLECT_TIME_DEBUG
    printf("cc: nsCycleCollector::PrepareForCollection()\n");
#endif
    mCollectionStart = TimeStamp::Now();
    mVisitedRefCounted = 0;
    mVisitedGCed = 0;

    mCollectionInProgress = true;

    nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
    if (obs)
        obs->NotifyObservers(nsnull, "cycle-collector-begin", nsnull);

    mFollowupCollection = false;
    mCollectedObjects = 0;

    mWhiteNodes = aWhiteNodes;

    return true;
}

void
nsCycleCollector::CleanupAfterCollection()
{
    mWhiteNodes = nsnull;
    mCollectionInProgress = false;

#ifdef XP_OS2
    // Now that the cycle collector has freed some memory, we can try to
    // force the C library to give back as much memory to the system as
    // possible.
    _heapmin();
#endif

    PRUint32 interval((TimeStamp::Now() - mCollectionStart).ToMilliseconds());
#ifdef COLLECT_TIME_DEBUG
    printf("cc: CleanupAfterCollection(), total time %ums\n", interval);
#endif
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR, interval);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_VISITED_REF_COUNTED, mVisitedRefCounted);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_VISITED_GCED, mVisitedGCed);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_COLLECTED, mWhiteNodeCount);

#ifdef DEBUG_CC
    ExplainLiveExpectedGarbage();
#endif
}

PRUint32
nsCycleCollector::Collect(PRUint32 aTryCollections,
                          nsICycleCollectorListener *aListener)
{
    nsAutoTArray<PtrInfo*, 4000> whiteNodes;

    if (!PrepareForCollection(&whiteNodes))
        return 0;

    PRUint32 totalCollections = 0;
    while (aTryCollections > totalCollections) {
        // Synchronous cycle collection. Always force a JS GC beforehand.
        GCIfNeeded(true);
        if (aListener && NS_FAILED(aListener->Begin()))
            aListener = nsnull;
        if (!(BeginCollection(aListener) &&
              FinishCollection(aListener)))
            break;

        ++totalCollections;
    }

    CleanupAfterCollection();

    return mCollectedObjects;
}

bool
nsCycleCollector::BeginCollection(nsICycleCollectorListener *aListener)
{
    // aListener should be Begin()'d before this
    if (mParams.mDoNothing)
        return false;

    GCGraphBuilder builder(mGraph, mRuntimes, aListener);
    if (!builder.Initialized())
        return false;

#ifdef COLLECT_TIME_DEBUG
    PRTime now = PR_Now();
#endif
    for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
        if (mRuntimes[i])
            mRuntimes[i]->BeginCycleCollection(builder, false);
    }

#ifdef COLLECT_TIME_DEBUG
    printf("cc: mRuntimes[*]->BeginCycleCollection() took %lldms\n",
           (PR_Now() - now) / PR_USEC_PER_MSEC);

    now = PR_Now();
#endif

#ifdef DEBUG_CC
    PRUint32 purpleStart = builder.Count();
#endif
    mScanInProgress = true;
    SelectPurple(builder);
#ifdef DEBUG_CC
    PRUint32 purpleEnd = builder.Count();

    if (purpleStart != purpleEnd) {
#ifndef __MINGW32__
        if (mParams.mHookMalloc)
            InitMemHook();
#endif
        if (mParams.mLogPointers && !mPtrLog)
            mPtrLog = fopen("pointer_log", "w");

        PRUint32 i = 0;
        NodePool::Enumerator queue(mGraph.mNodes);
        while (i++ < purpleStart) {
            queue.GetNext();
        }
        while (i++ < purpleEnd) {
            mStats.mForgetNode++;
            if (mParams.mLogPointers)
                fprintf(mPtrLog, "F %p\n", queue.GetNext()->mPointer);
        }
    }
#endif

#ifdef COLLECT_TIME_DEBUG
    printf("cc: SelectPurple() took %lldms\n",
           (PR_Now() - now) / PR_USEC_PER_MSEC);
#endif

    if (builder.Count() > 0) {
        // The main Bacon & Rajan collection algorithm.

#ifdef COLLECT_TIME_DEBUG
        now = PR_Now();
#endif

        MarkRoots(builder);

#ifdef COLLECT_TIME_DEBUG
        {
            PRTime then = PR_Now();
            printf("cc: MarkRoots() took %lldms\n",
                   (then - now) / PR_USEC_PER_MSEC);
            now = then;
        }
#endif

        ScanRoots();

#ifdef COLLECT_TIME_DEBUG
        printf("cc: ScanRoots() took %lldms\n",
               (PR_Now() - now) / PR_USEC_PER_MSEC);
#endif

        mScanInProgress = false;

        if (aListener) {
            aListener->BeginResults();

            NodePool::Enumerator etor(mGraph.mNodes);
            while (!etor.IsDone()) {
                PtrInfo *pi = etor.GetNext();
                if (pi->mColor == black &&
                    pi->mRefCount > 0 && pi->mRefCount < PR_UINT32_MAX &&
                    pi->mInternalRefs != pi->mRefCount) {
                    aListener->DescribeRoot((PRUint64)pi->mPointer,
                                            pi->mInternalRefs);
                }
            }
        }

#ifdef DEBUG_CC
        if (mFollowupCollection && purpleStart != purpleEnd) {
            PRUint32 i = 0;
            NodePool::Enumerator queue(mGraph.mNodes);
            while (i++ < purpleStart) {
                queue.GetNext();
            }
            while (i++ < purpleEnd) {
                PtrInfo *pi = queue.GetNext();
                if (pi->mColor == white) {
                    printf("nsCycleCollector: a later shutdown collection collected the additional\n"
                           "  suspect %p %s\n"
                           "  (which could be fixed by improving traversal)\n",
                           pi->mPointer, pi->mName);
                }
            }
        }
#endif

        for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
            if (mRuntimes[i])
                mRuntimes[i]->FinishTraverse();
        }
    }
    else {
        mScanInProgress = false;
    }

    return true;
}

bool
nsCycleCollector::FinishCollection(nsICycleCollectorListener *aListener)
{
#ifdef COLLECT_TIME_DEBUG
    PRTime now = PR_Now();
#endif

    bool collected = CollectWhite(aListener);

#ifdef COLLECT_TIME_DEBUG
    printf("cc: CollectWhite() took %lldms\n",
           (PR_Now() - now) / PR_USEC_PER_MSEC);
#endif

#ifdef DEBUG_CC
    mStats.mCollection++;
    if (mParams.mReportStats)
        mStats.Dump();
#endif

    for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
        if (mRuntimes[i])
            mRuntimes[i]->FinishCycleCollection();
    }

    mFollowupCollection = true;

#ifdef DEBUG_CC
    // We wait until after FinishCollection to check the white nodes because
    // some objects may outlive CollectWhite but then be freed by
    // FinishCycleCollection (like XPConnect's deferred release of native
    // objects).
    PRUint32 i, count = mWhiteNodes->Length();
    for (i = 0; i < count; ++i) {
        PtrInfo *pinfo = mWhiteNodes->ElementAt(i);
        if (pinfo->mLangID == nsIProgrammingLanguage::CPLUSPLUS &&
            mPurpleBuf.Exists(pinfo->mPointer)) {
            printf("nsCycleCollector: %s object @%p is still alive after\n"
                   "  calling RootAndUnlinkJSObjects, Unlink, and Unroot on"
                   " it!  This probably\n"
                   "  means the Unlink implementation was insufficient.\n",
                   pinfo->mName, pinfo->mPointer);
        }
    }
#endif

    mWhiteNodes->Clear();
    ClearGraph();

    mParams.mDoNothing = false;

    return collected;
}

PRUint32
nsCycleCollector::SuspectedCount()
{
    return mPurpleBuf.Count();
}

void
nsCycleCollector::Shutdown()
{
    // Here we want to run a final collection and then permanently
    // disable the collector because the program is shutting down.

    nsCOMPtr<nsCycleCollectorLogger> listener;
    if (mParams.mLogGraphs) {
        listener = new nsCycleCollectorLogger();
    }
    Collect(SHUTDOWN_COLLECTIONS(mParams), listener);

#ifdef DEBUG_CC
    GCGraphBuilder builder(mGraph, mRuntimes, nsnull);
    mScanInProgress = true;
    SelectPurple(builder);
    mScanInProgress = false;
    if (builder.Count() != 0) {
        printf("Might have been able to release more cycles if the cycle collector would "
               "run once more at shutdown.\n");
    }
    ClearGraph();
#endif
    mParams.mDoNothing = true;
}

#ifdef DEBUG_CC

static PLDHashOperator
AddExpectedGarbage(nsVoidPtrHashKey *p, void *arg)
{
    GCGraphBuilder *builder = static_cast<GCGraphBuilder*>(arg);
    nsISupports *root =
      static_cast<nsISupports*>(const_cast<void*>(p->GetKey()));
    builder->NoteXPCOMRoot(root);
    return PL_DHASH_NEXT;
}

struct SetSCCVisitor
{
    SetSCCVisitor(PRUint32 aIndex) : mIndex(aIndex) {}
    bool ShouldVisitNode(PtrInfo const *pi) { return pi->mSCCIndex == 0; }
    void VisitNode(PtrInfo *pi) { pi->mSCCIndex = mIndex; }
private:
    PRUint32 mIndex;
};

struct SetNonRootGreyVisitor
{
    bool ShouldVisitNode(PtrInfo const *pi) { return pi->mColor == white; }
    void VisitNode(PtrInfo *pi) { pi->mColor = grey; }
};

static void
PrintPathToExpectedGarbage(PtrInfo *pi)
{
    printf("  An object expected to be garbage could be "
           "reached from it by the path:\n");
    for (PtrInfo *path = pi, *prev = nsnull; prev != path;
         prev = path,
         path = path->mShortestPathToExpectedGarbage) {
        if (prev) {
            nsCString *edgeName = prev
                ->mShortestPathToExpectedGarbageEdgeName;
            printf("        via %s\n",
                   edgeName->IsEmpty() ? "<unknown edge>"
                                       : edgeName->get());
        }
        printf("    %s %p\n", path->mName, path->mPointer);
    }
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

    mCollectionInProgress = true;
    mScanInProgress = true;

    {
        GCGraphBuilder builder(mGraph, mRuntimes, nsnull);

        // Instead of adding roots from the purple buffer, we add them
        // from the list of nodes we were expected to collect.
        // Put the expected garbage in *before* calling
        // BeginCycleCollection so that we can separate the expected
        // garbage from the NoteRoot calls in such a way that something
        // that's in both is considered expected garbage.
        mExpectedGarbage.EnumerateEntries(&AddExpectedGarbage, &builder);

        PRUint32 expectedGarbageCount = builder.Count();

        for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
            if (mRuntimes[i])
                mRuntimes[i]->BeginCycleCollection(builder, true);
        }

        // But just for extra information, add entries from the purple
        // buffer too, since it may give us extra information about
        // traversal deficiencies.
        mPurpleBuf.NoteAll(builder);

        MarkRoots(builder);
        ScanRoots();

        mScanInProgress = false;

        for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
            if (mRuntimes[i]) {
                mRuntimes[i]->FinishTraverse();
            }
        }

        bool describeExtraRefcounts = false;
        bool findCycleRoots = false;
        {
            NodePool::Enumerator queue(mGraph.mNodes);
            PRUint32 i = 0;
            while (!queue.IsDone()) {
                PtrInfo *pi = queue.GetNext();
                if (pi->mColor == white) {
                    findCycleRoots = true;
                }

                if (pi->mInternalRefs != pi->mRefCount &&
                    (i < expectedGarbageCount || i >= mGraph.mRootCount)) {
                    // This check isn't particularly useful anymore
                    // given that we need to enter this part for i >=
                    // mGraph.mRootCount and there are plenty of
                    // NoteRoot roots.
                    describeExtraRefcounts = true;
                }
                ++i;
            }
        }

        if ((describeExtraRefcounts || findCycleRoots) &&
            CreateReversedEdges()) {
            // Note that the external references may have been external
            // to a different node in the cycle collection that just
            // happened, if that different node was purple and then
            // black.

            // Use mSCCIndex temporarily to track whether we've reached
            // nodes in the breadth-first search.
            const PRUint32 INDEX_UNREACHED = 0;
            const PRUint32 INDEX_REACHED = 1;
            NodePool::Enumerator etor_clear(mGraph.mNodes);
            while (!etor_clear.IsDone()) {
                PtrInfo *pi = etor_clear.GetNext();
                pi->mSCCIndex = INDEX_UNREACHED;
            }

            nsDeque queue; // for breadth-first search
            NodePool::Enumerator etor_roots(mGraph.mNodes);
            for (PRUint32 i = 0; i < mGraph.mRootCount; ++i) {
                PtrInfo *root_pi = etor_roots.GetNext();
                if (i < expectedGarbageCount) {
                    root_pi->mSCCIndex = INDEX_REACHED;
                    root_pi->mShortestPathToExpectedGarbage = root_pi;
                    queue.Push(root_pi);
                }
            }

            while (queue.GetSize() > 0) {
                PtrInfo *pi = (PtrInfo*)queue.PopFront();
                for (ReversedEdge *e = pi->mReversedEdges; e; e = e->mNext) {
                    if (e->mTarget->mSCCIndex == INDEX_UNREACHED) {
                        e->mTarget->mSCCIndex = INDEX_REACHED;
                        PtrInfo *target = e->mTarget;
                        if (!target->mShortestPathToExpectedGarbage) {
                            target->mShortestPathToExpectedGarbage = pi;
                            target->mShortestPathToExpectedGarbageEdgeName =
                                e->mEdgeName;
                        }
                        queue.Push(target);
                    }
                }

                if (pi->mRefCount == PR_UINT32_MAX ||
                    (pi->mInternalRefs != pi->mRefCount && pi->mRefCount > 0)) {
                    if (pi->mRefCount == PR_UINT32_MAX) {
                        printf("nsCycleCollector: %s %p was not collected due "
                           "to \n"
                           "  external references\n",
                           pi->mName, pi->mPointer);
                    }
                    else {
                        printf("nsCycleCollector: %s %p was not collected due "
                               "to %d\n"
                               "  external references (%d total - %d known)\n",
                               pi->mName, pi->mPointer,
                               pi->mRefCount - pi->mInternalRefs,
                               pi->mRefCount, pi->mInternalRefs);
                    }

                    PrintPathToExpectedGarbage(pi);

                    if (pi->mRefCount == PR_UINT32_MAX) {
                        printf("  The known references to it were from:\n");
                    }
                    else {
                        printf("  The %d known references to it were from:\n",
                               pi->mInternalRefs);
                    }
                    for (ReversedEdge *e = pi->mReversedEdges;
                         e; e = e->mNext) {
                        printf("    %s %p",
                               e->mTarget->mName, e->mTarget->mPointer);
                        if (!e->mEdgeName->IsEmpty()) {
                            printf(" via %s", e->mEdgeName->get());
                        }
                        printf("\n");
                    }
                    mRuntimes[pi->mLangID]->PrintAllReferencesTo(pi->mPointer);
                }
            }

            if (findCycleRoots) {
                // NOTE: This code changes the white nodes that are not
                // roots to gray.

                // Put the nodes in post-order traversal order from a
                // depth-first search.
                nsDeque DFSPostOrder;

                {
                    // Use mSCCIndex temporarily to track the DFS numbering:
                    const PRUint32 INDEX_UNREACHED = 0;
                    const PRUint32 INDEX_TRAVERSING = 1;
                    const PRUint32 INDEX_NUMBERED = 2;

                    NodePool::Enumerator etor_clear(mGraph.mNodes);
                    while (!etor_clear.IsDone()) {
                        PtrInfo *pi = etor_clear.GetNext();
                        pi->mSCCIndex = INDEX_UNREACHED;
                    }

                    nsDeque stack;

                    NodePool::Enumerator etor_roots(mGraph.mNodes);
                    for (PRUint32 i = 0; i < mGraph.mRootCount; ++i) {
                        PtrInfo *root_pi = etor_roots.GetNext();
                        stack.Push(root_pi);
                    }

                    while (stack.GetSize() > 0) {
                        PtrInfo *pi = (PtrInfo*)stack.Peek();
                        if (pi->mSCCIndex == INDEX_UNREACHED) {
                            pi->mSCCIndex = INDEX_TRAVERSING;
                            for (EdgePool::Iterator child = pi->FirstChild(),
                                                child_end = pi->LastChild();
                                 child != child_end; ++child) {
                                stack.Push(*child);
                            }
                        } else {
                            stack.Pop();
                            // Somebody else might have numbered it already
                            // (since this is depth-first, not breadth-first).
                            // This happens if a node is pushed on the stack
                            // a second time while it is on the stack in
                            // UNREACHED state.
                            if (pi->mSCCIndex == INDEX_TRAVERSING) {
                                pi->mSCCIndex = INDEX_NUMBERED;
                                DFSPostOrder.Push(pi);
                            }
                        }
                    }
                }

                // Put the nodes into strongly-connected components.
                {
                    NodePool::Enumerator etor_clear(mGraph.mNodes);
                    while (!etor_clear.IsDone()) {
                        PtrInfo *pi = etor_clear.GetNext();
                        pi->mSCCIndex = 0;
                    }

                    PRUint32 currentSCC = 1;

                    while (DFSPostOrder.GetSize() > 0) {
                        GraphWalker<SetSCCVisitor>(SetSCCVisitor(currentSCC)).Walk((PtrInfo*)DFSPostOrder.PopFront());
                        ++currentSCC;
                    }
                }

                // Mark any white nodes reachable from other components as
                // grey.
                {
                    NodePool::Enumerator queue(mGraph.mNodes);
                    while (!queue.IsDone()) {
                        PtrInfo *pi = queue.GetNext();
                        if (pi->mColor != white)
                            continue;
                        for (EdgePool::Iterator child = pi->FirstChild(),
                                            child_end = pi->LastChild();
                             child != child_end; ++child) {
                            if ((*child)->mSCCIndex != pi->mSCCIndex) {
                                GraphWalker<SetNonRootGreyVisitor>(SetNonRootGreyVisitor()).Walk(*child);
                            }
                        }
                    }
                }

                {
                    NodePool::Enumerator queue(mGraph.mNodes);
                    while (!queue.IsDone()) {
                        PtrInfo *pi = queue.GetNext();
                        if (pi->mColor == white) {
                            if (pi->mLangID ==
                                    nsIProgrammingLanguage::CPLUSPLUS &&
                                mPurpleBuf.Exists(pi->mPointer)) {
                                printf(
"nsCycleCollector: %s %p in component %d\n"
"  which was reference counted during the root/unlink/unroot phase of the\n"
"  last collection was not collected due to failure to unlink (see other\n"
"  warnings) or deficiency in traverse that causes cycles referenced only\n"
"  from other cycles to require multiple rounds of cycle collection in which\n"
"  this object was likely the reachable object\n",
                                       pi->mName, pi->mPointer, pi->mSCCIndex);
                            } else {
                                printf(
"nsCycleCollector: %s %p in component %d\n"
"  was not collected due to missing call to suspect, failure to unlink (see\n"
"  other warnings), or deficiency in traverse that causes cycles referenced\n"
"  only from other cycles to require multiple rounds of cycle collection\n",
                                       pi->mName, pi->mPointer, pi->mSCCIndex);
                            }
                            if (pi->mShortestPathToExpectedGarbage)
                                PrintPathToExpectedGarbage(pi);
                        }
                    }
                }
            }

            DestroyReversedEdges();
        }
    }

    ClearGraph();

    mCollectionInProgress = false;

    for (PRUint32 i = 0; i <= nsIProgrammingLanguage::MAX; ++i) {
        if (mRuntimes[i])
            mRuntimes[i]->FinishCycleCollection();
    }    
}

bool
nsCycleCollector::CreateReversedEdges()
{
    // Count the edges in the graph.
    PRUint32 edgeCount = 0;
    NodePool::Enumerator countQueue(mGraph.mNodes);
    while (!countQueue.IsDone()) {
        PtrInfo *pi = countQueue.GetNext();
        for (EdgePool::Iterator e = pi->FirstChild(), e_end = pi->LastChild();
             e != e_end; ++e, ++edgeCount) {
        }
    }

    // Allocate a pool to hold all of the edges.
    mGraph.mReversedEdges = new ReversedEdge[edgeCount];
    if (mGraph.mReversedEdges == nsnull) {
        NS_NOTREACHED("allocation failure creating reversed edges");
        return false;
    }

    // Fill in the reversed edges by scanning all forward edges.
    ReversedEdge *current = mGraph.mReversedEdges;
    NodePool::Enumerator buildQueue(mGraph.mNodes);
    while (!buildQueue.IsDone()) {
        PtrInfo *pi = buildQueue.GetNext();
        PRInt32 i = 0;
        for (EdgePool::Iterator e = pi->FirstChild(), e_end = pi->LastChild();
             e != e_end; ++e) {
            current->mTarget = pi;
            current->mEdgeName = &pi->mEdgeNames[i];
            current->mNext = (*e)->mReversedEdges;
            (*e)->mReversedEdges = current;
            ++current;
            ++i;
        }
    }
    NS_ASSERTION(current - mGraph.mReversedEdges == ptrdiff_t(edgeCount),
                 "misallocation");
    return true;
}

void
nsCycleCollector::DestroyReversedEdges()
{
    NodePool::Enumerator queue(mGraph.mNodes);
    while (!queue.IsDone()) {
        PtrInfo *pi = queue.GetNext();
        pi->mReversedEdges = nsnull;
    }

    delete mGraph.mReversedEdges;
    mGraph.mReversedEdges = nsnull;
}

void
nsCycleCollector::ShouldBeFreed(nsISupports *n)
{
    if (n) {
        mExpectedGarbage.PutEntry(n);
    }
}

void
nsCycleCollector::WasFreed(nsISupports *n)
{
    if (n) {
        mExpectedGarbage.RemoveEntry(n);
    }
}
#endif


////////////////////////
// Memory reporter
////////////////////////

static PRInt64
ReportCycleCollectorMem()
{
    if (!sCollector)
        return 0;
    PRInt64 size = sizeof(nsCycleCollector) + 
        sCollector->mPurpleBuf.BlocksSize() +
        sCollector->mGraph.BlocksSize();
    if (sCollector->mWhiteNodes)
        size += sCollector->mWhiteNodes->Capacity() * sizeof(PtrInfo*);
    return size;
}

NS_MEMORY_REPORTER_IMPLEMENT(CycleCollector,
                             "explicit/cycle-collector",
                             KIND_HEAP,
                             UNITS_BYTES,
                             ReportCycleCollectorMem,
                             "Memory used by the cycle collector.  This "
                             "includes the cycle collector structure, the "
                             "purple buffer, the graph, and the white nodes.  "
                             "The latter two are expected to be empty when the "
                             "cycle collector is idle.")


////////////////////////////////////////////////////////////////////////
// Module public API (exported in nsCycleCollector.h)
// Just functions that redirect into the singleton, once it's built.
////////////////////////////////////////////////////////////////////////

void 
nsCycleCollector_registerRuntime(PRUint32 langID, 
                                 nsCycleCollectionLanguageRuntime *rt)
{
    static bool regMemReport = true;
    if (sCollector)
        sCollector->RegisterRuntime(langID, rt);
    if (regMemReport) {
        regMemReport = false;
        NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(CycleCollector));
    }
}

nsCycleCollectionLanguageRuntime *
nsCycleCollector_getRuntime(PRUint32 langID)
{
    if (sCollector)
        sCollector->GetRuntime(langID);
    return nsnull;
}

void 
nsCycleCollector_forgetRuntime(PRUint32 langID)
{
    if (sCollector)
        sCollector->ForgetRuntime(langID);
}


bool
NS_CycleCollectorSuspect(nsISupports *n)
{
    if (sCollector)
        return sCollector->Suspect(n);
    return false;
}

bool
NS_CycleCollectorForget(nsISupports *n)
{
    return sCollector ? sCollector->Forget(n) : true;
}

nsPurpleBufferEntry*
NS_CycleCollectorSuspect2(nsISupports *n)
{
    if (sCollector)
        return sCollector->Suspect2(n);
    return nsnull;
}

bool
NS_CycleCollectorForget2(nsPurpleBufferEntry *e)
{
    return sCollector ? sCollector->Forget2(e) : true;
}

PRUint32
nsCycleCollector_suspectedCount()
{
    return sCollector ? sCollector->SuspectedCount() : 0;
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

class nsCycleCollectorRunner : public nsRunnable
{
    nsCycleCollector *mCollector;
    nsICycleCollectorListener *mListener;
    Mutex mLock;
    CondVar mRequest;
    CondVar mReply;
    bool mRunning;
    bool mShutdown;
    bool mCollected;

    nsCycleCollectionJSRuntime *GetJSRuntime()
    {
        return static_cast<nsCycleCollectionJSRuntime*>
                 (mCollector->mRuntimes[nsIProgrammingLanguage::JAVASCRIPT]);
    }

public:
    NS_IMETHOD Run()
    {
#ifdef XP_WIN
        TlsSetValue(gTLSThreadIDIndex,
                    (void*) mozilla::threads::CycleCollector);
#elif defined(NS_TLS)
        gTLSThreadID = mozilla::threads::CycleCollector;
#else
        gCycleCollectorThread = PR_GetCurrentThread();
#endif

        NS_ASSERTION(NS_IsCycleCollectorThread() && !NS_IsMainThread(),
                     "Wrong thread!");

        MutexAutoLock autoLock(mLock);

        if (mShutdown)
            return NS_OK;

        mRunning = true;

        while (1) {
            mRequest.Wait();

            if (!mRunning) {
                mReply.Notify();
                return NS_OK;
            }

            GetJSRuntime()->NotifyEnterCycleCollectionThread();
            mCollected = mCollector->BeginCollection(mListener);
            GetJSRuntime()->NotifyLeaveCycleCollectionThread();

            mReply.Notify();
        }

        return NS_OK;
    }

    nsCycleCollectorRunner(nsCycleCollector *collector)
        : mCollector(collector),
          mListener(nsnull),
          mLock("cycle collector lock"),
          mRequest(mLock, "cycle collector request condvar"),
          mReply(mLock, "cycle collector reply condvar"),
          mRunning(false),
          mShutdown(false),
          mCollected(false)
    {
        NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    }

    PRUint32 Collect(nsICycleCollectorListener* aListener)
    {
        NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

        mCollector->GCIfNeeded(false);

        MutexAutoLock autoLock(mLock);

        if (!mRunning)
            return 0;

        nsAutoTArray<PtrInfo*, 4000> whiteNodes;
        if (!mCollector->PrepareForCollection(&whiteNodes))
            return 0;

        NS_ASSERTION(!mListener, "Should have cleared this already!");
        if (aListener && NS_FAILED(aListener->Begin()))
            aListener = nsnull;
        mListener = aListener;

        GetJSRuntime()->NotifyLeaveMainThread();
        mRequest.Notify();
        mReply.Wait();
        GetJSRuntime()->NotifyEnterMainThread();

        mListener = nsnull;

        if (mCollected) {
            mCollected = mCollector->FinishCollection(aListener);

            mCollector->CleanupAfterCollection();

            return mCollected ? mCollector->mCollectedObjects : 0;
        }

        return 0;
    }

    void Shutdown()
    {
        NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

        MutexAutoLock autoLock(mLock);

        mShutdown = true;

        if (!mRunning)
            return;

        mRunning = false;
        mRequest.Notify();
        mReply.Wait();
    }
};

// Holds a reference.
static nsCycleCollectorRunner* sCollectorRunner;

// Holds a reference.
static nsIThread* sCollectorThread;

nsresult
nsCycleCollector_startup()
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(!sCollector, "Forgot to call nsCycleCollector_shutdown?");

    sCollector = new nsCycleCollector();

    nsRefPtr<nsCycleCollectorRunner> runner =
        new nsCycleCollectorRunner(sCollector);

    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewThread(getter_AddRefs(thread), runner);
    NS_ENSURE_SUCCESS(rv, rv);

    runner.swap(sCollectorRunner);
    thread.swap(sCollectorThread);

    return rv;
}

PRUint32
nsCycleCollector_collect(nsICycleCollectorListener *aListener)
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsCOMPtr<nsICycleCollectorListener> listener(aListener);
    if (!aListener && sCollector && sCollector->mParams.mLogGraphs) {
        listener = new nsCycleCollectorLogger();
    }

    if (sCollectorRunner)
        return sCollectorRunner->Collect(listener);
    return sCollector ? sCollector->Collect(1, listener) : 0;
}

void
nsCycleCollector_shutdownThreads()
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    if (sCollectorRunner) {
        nsRefPtr<nsCycleCollectorRunner> runner;
        runner.swap(sCollectorRunner);
        runner->Shutdown();
    }

    if (sCollectorThread) {
        nsCOMPtr<nsIThread> thread;
        thread.swap(sCollectorThread);
        thread->Shutdown();
    }
}

void
nsCycleCollector_shutdown()
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(!sCollectorRunner, "Should have finished before!");
    NS_ASSERTION(!sCollectorThread, "Should have finished before!");

    if (sCollector) {
        sCollector->Shutdown();
        delete sCollector;
        sCollector = nsnull;
    }
}
