/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
// Snow-white is an addition to the original algorithm. Snow-white object
// has reference count zero and is just waiting for deletion.
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

// Incremental cycle collection
//
// Beyond the simple state machine required to implement incremental
// collection, the CC needs to be able to compensate for things the browser
// is doing during the collection. There are two kinds of problems. For each
// of these, there are two cases to deal with: purple-buffered C++ objects
// and JS objects.

// The first problem is that an object in the CC's graph can become garbage.
// This is bad because the CC touches the objects in its graph at every
// stage of its operation.
//
// All cycle collected C++ objects that die during a cycle collection
// will end up actually getting deleted by the SnowWhiteKiller. Before
// the SWK deletes an object, it checks if an ICC is running, and if so,
// if the object is in the graph. If it is, the CC clears mPointer and
// mParticipant so it does not point to the raw object any more. Because
// objects could die any time the CC returns to the mutator, any time the CC
// accesses a PtrInfo it must perform a null check on mParticipant to
// ensure the object has not gone away.
//
// JS objects don't always run finalizers, so the CC can't remove them from
// the graph when they die. Fortunately, JS objects can only die during a GC,
// so if a GC is begun during an ICC, the browser synchronously finishes off
// the ICC, which clears the entire CC graph. If the GC and CC are scheduled
// properly, this should be rare.
//
// The second problem is that objects in the graph can be changed, say by
// being addrefed or released, or by having a field updated, after the object
// has been added to the graph. The problem is that ICC can miss a newly
// created reference to an object, and end up unlinking an object that is
// actually alive.
//
// The basic idea of the solution, from "An on-the-fly Reference Counting
// Garbage Collector for Java" by Levanoni and Petrank, is to notice if an
// object has had an additional reference to it created during the collection,
// and if so, don't collect it during the current collection. This avoids having
// to rerun the scan as in Bacon & Rajan 2001.
//
// For cycle collected C++ objects, we modify AddRef to place the object in
// the purple buffer, in addition to Release. Then, in the CC, we treat any
// objects in the purple buffer as being alive, after graph building has
// completed. Because they are in the purple buffer, they will be suspected
// in the next CC, so there's no danger of leaks. This is imprecise, because
// we will treat as live an object that has been Released but not AddRefed
// during graph building, but that's probably rare enough that the additional
// bookkeeping overhead is not worthwhile.
//
// For JS objects, the cycle collector is only looking at gray objects. If a
// gray object is touched during ICC, it will be made black by UnmarkGray.
// Thus, if a JS object has become black during the ICC, we treat it as live.
// Merged JS zones have to be handled specially: we scan all zone globals.
// If any are black, we treat the zone as being black.


// Safety
//
// An XPCOM object is either scan-safe or scan-unsafe, purple-safe or
// purple-unsafe.
//
// An nsISupports object is scan-safe if:
//
//  - It can be QI'ed to |nsXPCOMCycleCollectionParticipant|, though
//    this operation loses ISupports identity (like nsIClassInfo).
//  - Additionally, the operation |traverse| on the resulting
//    nsXPCOMCycleCollectionParticipant does not cause *any* refcount
//    adjustment to occur (no AddRef / Release calls).
//
// A non-nsISupports ("native") object is scan-safe by explicitly
// providing its nsCycleCollectionParticipant.
//
// An object is purple-safe if it satisfies the following properties:
//
//  - The object is scan-safe.
//
// When we receive a pointer |ptr| via
// |nsCycleCollector::suspect(ptr)|, we assume it is purple-safe. We
// can check the scan-safety, but have no way to ensure the
// purple-safety; objects must obey, or else the entire system falls
// apart. Don't involve an object in this scheme if you can't
// guarantee its purple-safety. The easiest way to ensure that an
// object is purple-safe is to use nsCycleCollectingAutoRefCnt.
//
// When we have a scannable set of purple nodes ready, we begin
// our walks. During the walks, the nodes we |traverse| should only
// feed us more scan-safe nodes, and should not adjust the refcounts
// of those nodes.
//
// We do not |AddRef| or |Release| any objects during scanning. We
// rely on the purple-safety of the roots that call |suspect| to
// hold, such that we will clear the pointer from the purple buffer
// entry to the object before it is destroyed. The pointers that are
// merely scan-safe we hold only for the duration of scanning, and
// there should be no objects released from the scan-safe set during
// the scan.
//
// We *do* call |Root| and |Unroot| on every white object, on
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

#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/HoldDropJSObjects.h"
/* This must occur *after* base/process_util.h to avoid typedefs conflicts. */
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SegmentedVector.h"

#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsDeque.h"
#include "nsCycleCollector.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prenv.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsIConsoleService.h"
#include "mozilla/Attributes.h"
#include "nsICycleCollectorListener.h"
#include "nsIMemoryReporter.h"
#include "nsIFile.h"
#include "nsDumpUtils.h"
#include "xpcpublic.h"
#include "GeckoProfiler.h"
#include <stdint.h>
#include <stdio.h>

#include "mozilla/AutoGlobalTimelineMarker.h"
#include "mozilla/Likely.h"
#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThreadLocal.h"

using namespace mozilla;

//#define COLLECT_TIME_DEBUG

// Enable assertions that are useful for diagnosing errors in graph construction.
//#define DEBUG_CC_GRAPH

#define DEFAULT_SHUTDOWN_COLLECTIONS 5

// One to do the freeing, then another to detect there is no more work to do.
#define NORMAL_SHUTDOWN_COLLECTIONS 2

// Cycle collector environment variables
//
// MOZ_CC_LOG_ALL: If defined, always log cycle collector heaps.
//
// MOZ_CC_LOG_SHUTDOWN: If defined, log cycle collector heaps at shutdown.
//
// MOZ_CC_LOG_THREAD: If set to "main", only automatically log main thread
// CCs. If set to "worker", only automatically log worker CCs. If set to "all",
// log either. The default value is "all". This must be used with either
// MOZ_CC_LOG_ALL or MOZ_CC_LOG_SHUTDOWN for it to do anything.
//
// MOZ_CC_LOG_PROCESS: If set to "main", only automatically log main process
// CCs. If set to "content", only automatically log tab CCs. If set to
// "plugins", only automatically log plugin CCs. If set to "all", log
// everything. The default value is "all". This must be used with either
// MOZ_CC_LOG_ALL or MOZ_CC_LOG_SHUTDOWN for it to do anything.
//
// MOZ_CC_ALL_TRACES: If set to "all", any cycle collector
// logging done will be WantAllTraces, which disables
// various cycle collector optimizations to give a fuller picture of
// the heap. If set to "shutdown", only shutdown logging will be WantAllTraces.
// The default is none.
//
// MOZ_CC_RUN_DURING_SHUTDOWN: In non-DEBUG or builds, if this is set,
// run cycle collections at shutdown.
//
// MOZ_CC_LOG_DIRECTORY: The directory in which logs are placed (such as
// logs from MOZ_CC_LOG_ALL and MOZ_CC_LOG_SHUTDOWN, or other uses
// of nsICycleCollectorListener)

// Various parameters of this collector can be tuned using environment
// variables.

struct nsCycleCollectorParams
{
  bool mLogAll;
  bool mLogShutdown;
  bool mAllTracesAll;
  bool mAllTracesShutdown;
  bool mLogThisThread;

  nsCycleCollectorParams() :
    mLogAll(PR_GetEnv("MOZ_CC_LOG_ALL") != nullptr),
    mLogShutdown(PR_GetEnv("MOZ_CC_LOG_SHUTDOWN") != nullptr),
    mAllTracesAll(false),
    mAllTracesShutdown(false)
  {
    const char* logThreadEnv = PR_GetEnv("MOZ_CC_LOG_THREAD");
    bool threadLogging = true;
    if (logThreadEnv && !!strcmp(logThreadEnv, "all")) {
      if (NS_IsMainThread()) {
        threadLogging = !strcmp(logThreadEnv, "main");
      } else {
        threadLogging = !strcmp(logThreadEnv, "worker");
      }
    }

    const char* logProcessEnv = PR_GetEnv("MOZ_CC_LOG_PROCESS");
    bool processLogging = true;
    if (logProcessEnv && !!strcmp(logProcessEnv, "all")) {
      switch (XRE_GetProcessType()) {
        case GeckoProcessType_Default:
          processLogging = !strcmp(logProcessEnv, "main");
          break;
        case GeckoProcessType_Plugin:
          processLogging = !strcmp(logProcessEnv, "plugins");
          break;
        case GeckoProcessType_Content:
          processLogging = !strcmp(logProcessEnv, "content");
          break;
        default:
          processLogging = false;
          break;
      }
    }
    mLogThisThread = threadLogging && processLogging;

    const char* allTracesEnv = PR_GetEnv("MOZ_CC_ALL_TRACES");
    if (allTracesEnv) {
      if (!strcmp(allTracesEnv, "all")) {
        mAllTracesAll = true;
      } else if (!strcmp(allTracesEnv, "shutdown")) {
        mAllTracesShutdown = true;
      }
    }
  }

  bool LogThisCC(bool aIsShutdown)
  {
    return (mLogAll || (aIsShutdown && mLogShutdown)) && mLogThisThread;
  }

  bool AllTracesThisCC(bool aIsShutdown)
  {
    return mAllTracesAll || (aIsShutdown && mAllTracesShutdown);
  }
};

#ifdef COLLECT_TIME_DEBUG
class TimeLog
{
public:
  TimeLog() : mLastCheckpoint(TimeStamp::Now())
  {
  }

  void
  Checkpoint(const char* aEvent)
  {
    TimeStamp now = TimeStamp::Now();
    double dur = (now - mLastCheckpoint).ToMilliseconds();
    if (dur >= 0.5) {
      printf("cc: %s took %.1fms\n", aEvent, dur);
    }
    mLastCheckpoint = now;
  }

private:
  TimeStamp mLastCheckpoint;
};
#else
class TimeLog
{
public:
  TimeLog()
  {
  }
  void Checkpoint(const char* aEvent)
  {
  }
};
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
    mSentinelAndBlocks[0].block = nullptr;
    mSentinelAndBlocks[1].block = nullptr;
  }

  ~EdgePool()
  {
    MOZ_ASSERT(!mSentinelAndBlocks[0].block &&
               !mSentinelAndBlocks[1].block,
               "Didn't call Clear()?");
  }

  void Clear()
  {
    Block* b = Blocks();
    while (b) {
      Block* next = b->Next();
      delete b;
      b = next;
    }

    mSentinelAndBlocks[0].block = nullptr;
    mSentinelAndBlocks[1].block = nullptr;
  }

#ifdef DEBUG
  bool IsEmpty()
  {
    return !mSentinelAndBlocks[0].block &&
           !mSentinelAndBlocks[1].block;
  }
#endif

private:
  struct Block;
  union PtrInfoOrBlock
  {
    // Use a union to avoid reinterpret_cast and the ensuing
    // potential aliasing bugs.
    PtrInfo* ptrInfo;
    Block* block;
  };
  struct Block
  {
    enum { BlockSize = 16 * 1024 };

    PtrInfoOrBlock mPointers[BlockSize];
    Block()
    {
      mPointers[BlockSize - 2].block = nullptr; // sentinel
      mPointers[BlockSize - 1].block = nullptr; // next block pointer
    }
    Block*& Next()
    {
      return mPointers[BlockSize - 1].block;
    }
    PtrInfoOrBlock* Start()
    {
      return &mPointers[0];
    }
    PtrInfoOrBlock* End()
    {
      return &mPointers[BlockSize - 2];
    }
  };

  // Store the null sentinel so that we can have valid iterators
  // before adding any edges and without adding any blocks.
  PtrInfoOrBlock mSentinelAndBlocks[2];

  Block*& Blocks()
  {
    return mSentinelAndBlocks[1].block;
  }
  Block* Blocks() const
  {
    return mSentinelAndBlocks[1].block;
  }

public:
  class Iterator
  {
  public:
    Iterator() : mPointer(nullptr) {}
    explicit Iterator(PtrInfoOrBlock* aPointer) : mPointer(aPointer) {}
    Iterator(const Iterator& aOther) : mPointer(aOther.mPointer) {}

    Iterator& operator++()
    {
      if (!mPointer->ptrInfo) {
        // Null pointer is a sentinel for link to the next block.
        mPointer = (mPointer + 1)->block->mPointers;
      }
      ++mPointer;
      return *this;
    }

    PtrInfo* operator*() const
    {
      if (!mPointer->ptrInfo) {
        // Null pointer is a sentinel for link to the next block.
        return (mPointer + 1)->block->mPointers->ptrInfo;
      }
      return mPointer->ptrInfo;
    }
    bool operator==(const Iterator& aOther) const
    {
      return mPointer == aOther.mPointer;
    }
    bool operator!=(const Iterator& aOther) const
    {
      return mPointer != aOther.mPointer;
    }

#ifdef DEBUG_CC_GRAPH
    bool Initialized() const
    {
      return mPointer != nullptr;
    }
#endif

  private:
    PtrInfoOrBlock* mPointer;
  };

  class Builder;
  friend class Builder;
  class Builder
  {
  public:
    explicit Builder(EdgePool& aPool)
      : mCurrent(&aPool.mSentinelAndBlocks[0])
      , mBlockEnd(&aPool.mSentinelAndBlocks[0])
      , mNextBlockPtr(&aPool.Blocks())
    {
    }

    Iterator Mark()
    {
      return Iterator(mCurrent);
    }

    void Add(PtrInfo* aEdge)
    {
      if (mCurrent == mBlockEnd) {
        Block* b = new Block();
        *mNextBlockPtr = b;
        mCurrent = b->Start();
        mBlockEnd = b->End();
        mNextBlockPtr = &b->Next();
      }
      (mCurrent++)->ptrInfo = aEdge;
    }
  private:
    // mBlockEnd points to space for null sentinel
    PtrInfoOrBlock* mCurrent;
    PtrInfoOrBlock* mBlockEnd;
    Block** mNextBlockPtr;
  };

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t n = 0;
    Block* b = Blocks();
    while (b) {
      n += aMallocSizeOf(b);
      b = b->Next();
    }
    return n;
  }
};

#ifdef DEBUG_CC_GRAPH
#define CC_GRAPH_ASSERT(b) MOZ_ASSERT(b)
#else
#define CC_GRAPH_ASSERT(b)
#endif

#define CC_TELEMETRY(_name, _value)                                            \
    PR_BEGIN_MACRO                                                             \
    if (NS_IsMainThread()) {                                                   \
      Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR##_name, _value);        \
    } else {                                                                   \
      Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_WORKER##_name, _value); \
    }                                                                          \
    PR_END_MACRO

enum NodeColor { black, white, grey };

// This structure should be kept as small as possible; we may expect
// hundreds of thousands of them to be allocated and touched
// repeatedly during each cycle collection.

struct PtrInfo
{
  void* mPointer;
  nsCycleCollectionParticipant* mParticipant;
  uint32_t mColor : 2;
  uint32_t mInternalRefs : 30;
  uint32_t mRefCount;
private:
  EdgePool::Iterator mFirstChild;

  static const uint32_t kInitialRefCount = UINT32_MAX - 1;

public:

  PtrInfo(void* aPointer, nsCycleCollectionParticipant* aParticipant)
    : mPointer(aPointer),
      mParticipant(aParticipant),
      mColor(grey),
      mInternalRefs(0),
      mRefCount(kInitialRefCount),
      mFirstChild()
  {
    MOZ_ASSERT(aParticipant);

    // We initialize mRefCount to a large non-zero value so
    // that it doesn't look like a JS object to the cycle collector
    // in the case where the object dies before being traversed.
    MOZ_ASSERT(!IsGrayJS() && !IsBlackJS());
  }

  // Allow NodePool::Block's constructor to compile.
  PtrInfo()
  {
    NS_NOTREACHED("should never be called");
  }

  bool IsGrayJS() const
  {
    return mRefCount == 0;
  }

  bool IsBlackJS() const
  {
    return mRefCount == UINT32_MAX;
  }

  bool WasTraversed() const
  {
    return mRefCount != kInitialRefCount;
  }

  EdgePool::Iterator FirstChild() const
  {
    CC_GRAPH_ASSERT(mFirstChild.Initialized());
    return mFirstChild;
  }

  // this PtrInfo must be part of a NodePool
  EdgePool::Iterator LastChild() const
  {
    CC_GRAPH_ASSERT((this + 1)->mFirstChild.Initialized());
    return (this + 1)->mFirstChild;
  }

  void SetFirstChild(EdgePool::Iterator aFirstChild)
  {
    CC_GRAPH_ASSERT(aFirstChild.Initialized());
    mFirstChild = aFirstChild;
  }

  // this PtrInfo must be part of a NodePool
  void SetLastChild(EdgePool::Iterator aLastChild)
  {
    CC_GRAPH_ASSERT(aLastChild.Initialized());
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
  // The -2 allows us to use |BlockSize + 1| for |mEntries|, and fit |mNext|,
  // all without causing slop.
  enum { BlockSize = 8 * 1024 - 2 };

  struct Block
  {
    // We create and destroy Block using moz_xmalloc/free rather
    // than new and delete to avoid calling its constructor and
    // destructor.
    Block()
    {
      NS_NOTREACHED("should never be called");

      // Ensure Block is the right size (see the comment on BlockSize above).
      static_assert(
        sizeof(Block) == 163824 ||      // 32-bit; equals 39.997 pages
        sizeof(Block) == 262120,        // 64-bit; equals 63.994 pages
        "ill-sized NodePool::Block"
      );
    }
    ~Block()
    {
      NS_NOTREACHED("should never be called");
    }

    Block* mNext;
    PtrInfo mEntries[BlockSize + 1]; // +1 to store last child of last node
  };

public:
  NodePool()
    : mBlocks(nullptr)
    , mLast(nullptr)
  {
  }

  ~NodePool()
  {
    MOZ_ASSERT(!mBlocks, "Didn't call Clear()?");
  }

  void Clear()
  {
    Block* b = mBlocks;
    while (b) {
      Block* n = b->mNext;
      free(b);
      b = n;
    }

    mBlocks = nullptr;
    mLast = nullptr;
  }

#ifdef DEBUG
  bool IsEmpty()
  {
    return !mBlocks && !mLast;
  }
#endif

  class Builder;
  friend class Builder;
  class Builder
  {
  public:
    explicit Builder(NodePool& aPool)
      : mNextBlock(&aPool.mBlocks)
      , mNext(aPool.mLast)
      , mBlockEnd(nullptr)
    {
      MOZ_ASSERT(!aPool.mBlocks && !aPool.mLast, "pool not empty");
    }
    PtrInfo* Add(void* aPointer, nsCycleCollectionParticipant* aParticipant)
    {
      if (mNext == mBlockEnd) {
        Block* block = static_cast<Block*>(moz_xmalloc(sizeof(Block)));
        *mNextBlock = block;
        mNext = block->mEntries;
        mBlockEnd = block->mEntries + BlockSize;
        block->mNext = nullptr;
        mNextBlock = &block->mNext;
      }
      return new (mNext++) PtrInfo(aPointer, aParticipant);
    }
  private:
    Block** mNextBlock;
    PtrInfo*& mNext;
    PtrInfo* mBlockEnd;
  };

  class Enumerator;
  friend class Enumerator;
  class Enumerator
  {
  public:
    explicit Enumerator(NodePool& aPool)
      : mFirstBlock(aPool.mBlocks)
      , mCurBlock(nullptr)
      , mNext(nullptr)
      , mBlockEnd(nullptr)
      , mLast(aPool.mLast)
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
      MOZ_ASSERT(!IsDone(), "calling GetNext when done");
      if (mNext == mBlockEnd) {
        Block* nextBlock = mCurBlock ? mCurBlock->mNext : mFirstBlock;
        mNext = nextBlock->mEntries;
        mBlockEnd = mNext + BlockSize;
        mCurBlock = nextBlock;
      }
      return mNext++;
    }
  private:
    // mFirstBlock is a reference to allow an Enumerator to be constructed
    // for an empty graph.
    Block*& mFirstBlock;
    Block* mCurBlock;
    // mNext is the next value we want to return, unless mNext == mBlockEnd
    // NB: mLast is a reference to allow enumerating while building!
    PtrInfo* mNext;
    PtrInfo* mBlockEnd;
    PtrInfo*& mLast;
  };

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    // We don't measure the things pointed to by mEntries[] because those
    // pointers are non-owning.
    size_t n = 0;
    Block* b = mBlocks;
    while (b) {
      n += aMallocSizeOf(b);
      b = b->mNext;
    }
    return n;
  }

private:
  Block* mBlocks;
  PtrInfo* mLast;
};


// Declarations for mPtrToNodeMap.

struct PtrToNodeEntry : public PLDHashEntryHdr
{
  // The key is mNode->mPointer
  PtrInfo* mNode;
};

static bool
PtrToNodeMatchEntry(PLDHashTable* aTable,
                    const PLDHashEntryHdr* aEntry,
                    const void* aKey)
{
  const PtrToNodeEntry* n = static_cast<const PtrToNodeEntry*>(aEntry);
  return n->mNode->mPointer == aKey;
}

static PLDHashTableOps PtrNodeOps = {
  PLDHashTable::HashVoidPtrKeyStub,
  PtrToNodeMatchEntry,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  nullptr
};


struct WeakMapping
{
  // map and key will be null if the corresponding objects are GC marked
  PtrInfo* mMap;
  PtrInfo* mKey;
  PtrInfo* mKeyDelegate;
  PtrInfo* mVal;
};

class CCGraphBuilder;

struct CCGraph
{
  NodePool mNodes;
  EdgePool mEdges;
  nsTArray<WeakMapping> mWeakMaps;
  uint32_t mRootCount;

private:
  PLDHashTable mPtrToNodeMap;
  bool mOutOfMemory;

  static const uint32_t kInitialMapLength = 16384;

public:
  CCGraph()
    : mRootCount(0)
    , mPtrToNodeMap(&PtrNodeOps, sizeof(PtrToNodeEntry), kInitialMapLength)
    , mOutOfMemory(false)
  {}

  ~CCGraph() {}

  void Init()
  {
    MOZ_ASSERT(IsEmpty(), "Failed to call CCGraph::Clear");
  }

  void Clear()
  {
    mNodes.Clear();
    mEdges.Clear();
    mWeakMaps.Clear();
    mRootCount = 0;
    mPtrToNodeMap.ClearAndPrepareForLength(kInitialMapLength);
    mOutOfMemory = false;
  }

#ifdef DEBUG
  bool IsEmpty()
  {
    return mNodes.IsEmpty() && mEdges.IsEmpty() &&
           mWeakMaps.IsEmpty() && mRootCount == 0 &&
           mPtrToNodeMap.EntryCount() == 0;
  }
#endif

  PtrInfo* FindNode(void* aPtr);
  PtrToNodeEntry* AddNodeToMap(void* aPtr);
  void RemoveObjectFromMap(void* aObject);

  uint32_t MapCount() const
  {
    return mPtrToNodeMap.EntryCount();
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t n = 0;

    n += mNodes.SizeOfExcludingThis(aMallocSizeOf);
    n += mEdges.SizeOfExcludingThis(aMallocSizeOf);

    // We don't measure what the WeakMappings point to, because the
    // pointers are non-owning.
    n += mWeakMaps.ShallowSizeOfExcludingThis(aMallocSizeOf);

    n += mPtrToNodeMap.ShallowSizeOfExcludingThis(aMallocSizeOf);

    return n;
  }

private:
  PtrToNodeEntry* FindNodeEntry(void* aPtr)
  {
    return static_cast<PtrToNodeEntry*>(mPtrToNodeMap.Search(aPtr));
  }
};

PtrInfo*
CCGraph::FindNode(void* aPtr)
{
  PtrToNodeEntry* e = FindNodeEntry(aPtr);
  return e ? e->mNode : nullptr;
}

PtrToNodeEntry*
CCGraph::AddNodeToMap(void* aPtr)
{
  JS::AutoSuppressGCAnalysis suppress;
  if (mOutOfMemory) {
    return nullptr;
  }

  auto e = static_cast<PtrToNodeEntry*>(mPtrToNodeMap.Add(aPtr, fallible));
  if (!e) {
    mOutOfMemory = true;
    MOZ_ASSERT(false, "Ran out of memory while building cycle collector graph");
    return nullptr;
  }
  return e;
}

void
CCGraph::RemoveObjectFromMap(void* aObj)
{
  PtrToNodeEntry* e = FindNodeEntry(aObj);
  PtrInfo* pinfo = e ? e->mNode : nullptr;
  if (pinfo) {
    mPtrToNodeMap.RemoveEntry(e);

    pinfo->mPointer = nullptr;
    pinfo->mParticipant = nullptr;
  }
}


static nsISupports*
CanonicalizeXPCOMParticipant(nsISupports* aIn)
{
  nsISupports* out = nullptr;
  aIn->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                      reinterpret_cast<void**>(&out));
  return out;
}

static inline void
ToParticipant(nsISupports* aPtr, nsXPCOMCycleCollectionParticipant** aCp);

static void
CanonicalizeParticipant(void** aParti, nsCycleCollectionParticipant** aCp)
{
  // If the participant is null, this is an nsISupports participant,
  // so we must QI to get the real participant.

  if (!*aCp) {
    nsISupports* nsparti = static_cast<nsISupports*>(*aParti);
    nsparti = CanonicalizeXPCOMParticipant(nsparti);
    NS_ASSERTION(nsparti,
                 "Don't add objects that don't participate in collection!");
    nsXPCOMCycleCollectionParticipant* xcp;
    ToParticipant(nsparti, &xcp);
    *aParti = nsparti;
    *aCp = xcp;
  }
}

struct nsPurpleBufferEntry
{
  union
  {
    void* mObject;                        // when low bit unset
    nsPurpleBufferEntry* mNextInFreeList; // when low bit set
  };

  nsCycleCollectingAutoRefCnt* mRefCnt;

  nsCycleCollectionParticipant* mParticipant; // nullptr for nsISupports
};

class nsCycleCollector;

struct nsPurpleBuffer
{
private:
  struct Block
  {
    Block* mNext;
    // Try to match the size of a jemalloc bucket, to minimize slop bytes.
    // - On 32-bit platforms sizeof(nsPurpleBufferEntry) is 12, so mEntries
    //   is 16,380 bytes, which leaves 4 bytes for mNext.
    // - On 64-bit platforms sizeof(nsPurpleBufferEntry) is 24, so mEntries
    //   is 32,544 bytes, which leaves 8 bytes for mNext.
    nsPurpleBufferEntry mEntries[1365];

    Block() : mNext(nullptr)
    {
      // Ensure Block is the right size (see above).
      static_assert(
        sizeof(Block) == 16384 ||       // 32-bit
        sizeof(Block) == 32768,         // 64-bit
        "ill-sized nsPurpleBuffer::Block"
      );
    }

    template<class PurpleVisitor>
    void VisitEntries(nsPurpleBuffer& aBuffer, PurpleVisitor& aVisitor)
    {
      nsPurpleBufferEntry* eEnd = ArrayEnd(mEntries);
      for (nsPurpleBufferEntry* e = mEntries; e != eEnd; ++e) {
        MOZ_ASSERT(e->mObject, "There should be no null mObject when we iterate over the purple buffer");
        if (!(uintptr_t(e->mObject) & uintptr_t(1)) && e->mObject) {
          aVisitor.Visit(aBuffer, e);
        }
      }
    }
  };
  // This class wraps a linked list of the elements in the purple
  // buffer.

  uint32_t mCount;
  Block mFirstBlock;
  nsPurpleBufferEntry* mFreeList;

public:
  nsPurpleBuffer()
  {
    InitBlocks();
  }

  ~nsPurpleBuffer()
  {
    FreeBlocks();
  }

  template<class PurpleVisitor>
  void VisitEntries(PurpleVisitor& aVisitor)
  {
    for (Block* b = &mFirstBlock; b; b = b->mNext) {
      b->VisitEntries(*this, aVisitor);
    }
  }

  void InitBlocks()
  {
    mCount = 0;
    mFreeList = nullptr;
    StartBlock(&mFirstBlock);
  }

  void StartBlock(Block* aBlock)
  {
    MOZ_ASSERT(!mFreeList, "should not have free list");

    // Put all the entries in the block on the free list.
    nsPurpleBufferEntry* entries = aBlock->mEntries;
    mFreeList = entries;
    for (uint32_t i = 1; i < ArrayLength(aBlock->mEntries); ++i) {
      entries[i - 1].mNextInFreeList =
        (nsPurpleBufferEntry*)(uintptr_t(entries + i) | 1);
    }
    entries[ArrayLength(aBlock->mEntries) - 1].mNextInFreeList =
      (nsPurpleBufferEntry*)1;
  }

  void FreeBlocks()
  {
    if (mCount > 0) {
      UnmarkRemainingPurple(&mFirstBlock);
    }
    Block* b = mFirstBlock.mNext;
    while (b) {
      if (mCount > 0) {
        UnmarkRemainingPurple(b);
      }
      Block* next = b->mNext;
      delete b;
      b = next;
    }
    mFirstBlock.mNext = nullptr;
  }

  struct UnmarkRemainingPurpleVisitor
  {
    void
    Visit(nsPurpleBuffer& aBuffer, nsPurpleBufferEntry* aEntry)
    {
      if (aEntry->mRefCnt) {
        aEntry->mRefCnt->RemoveFromPurpleBuffer();
        aEntry->mRefCnt = nullptr;
      }
      aEntry->mObject = nullptr;
      --aBuffer.mCount;
    }
  };

  void UnmarkRemainingPurple(Block* aBlock)
  {
    UnmarkRemainingPurpleVisitor visitor;
    aBlock->VisitEntries(*this, visitor);
  }

  void SelectPointers(CCGraphBuilder& aBuilder);

  // RemoveSkippable removes entries from the purple buffer synchronously
  // (1) if aAsyncSnowWhiteFreeing is false and nsPurpleBufferEntry::mRefCnt is 0 or
  // (2) if the object's nsXPCOMCycleCollectionParticipant::CanSkip() returns true or
  // (3) if nsPurpleBufferEntry::mRefCnt->IsPurple() is false.
  // (4) If removeChildlessNodes is true, then any nodes in the purple buffer
  //     that will have no children in the cycle collector graph will also be
  //     removed. CanSkip() may be run on these children.
  void RemoveSkippable(nsCycleCollector* aCollector,
                       bool aRemoveChildlessNodes,
                       bool aAsyncSnowWhiteFreeing,
                       CC_ForgetSkippableCallback aCb);

  MOZ_ALWAYS_INLINE nsPurpleBufferEntry* NewEntry()
  {
    if (MOZ_UNLIKELY(!mFreeList)) {
      Block* b = new Block;
      StartBlock(b);

      // Add the new block as the second block in the list.
      b->mNext = mFirstBlock.mNext;
      mFirstBlock.mNext = b;
    }

    nsPurpleBufferEntry* e = mFreeList;
    mFreeList = (nsPurpleBufferEntry*)
      (uintptr_t(mFreeList->mNextInFreeList) & ~uintptr_t(1));
    return e;
  }

  MOZ_ALWAYS_INLINE void Put(void* aObject, nsCycleCollectionParticipant* aCp,
                             nsCycleCollectingAutoRefCnt* aRefCnt)
  {
    nsPurpleBufferEntry* e = NewEntry();

    ++mCount;

    e->mObject = aObject;
    e->mRefCnt = aRefCnt;
    e->mParticipant = aCp;
  }

  void Remove(nsPurpleBufferEntry* aEntry)
  {
    MOZ_ASSERT(mCount != 0, "must have entries");

    if (aEntry->mRefCnt) {
      aEntry->mRefCnt->RemoveFromPurpleBuffer();
      aEntry->mRefCnt = nullptr;
    }
    aEntry->mNextInFreeList =
      (nsPurpleBufferEntry*)(uintptr_t(mFreeList) | uintptr_t(1));
    mFreeList = aEntry;

    --mCount;
  }

  uint32_t Count() const
  {
    return mCount;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t n = 0;

    // Don't measure mFirstBlock because it's within |this|.
    const Block* block = mFirstBlock.mNext;
    while (block) {
      n += aMallocSizeOf(block);
      block = block->mNext;
    }

    // mFreeList is deliberately not measured because it points into
    // the purple buffer, which is within mFirstBlock and thus within |this|.
    //
    // We also don't measure the things pointed to by mEntries[] because
    // those pointers are non-owning.

    return n;
  }
};

static bool
AddPurpleRoot(CCGraphBuilder& aBuilder, void* aRoot,
              nsCycleCollectionParticipant* aParti);

struct SelectPointersVisitor
{
  explicit SelectPointersVisitor(CCGraphBuilder& aBuilder)
    : mBuilder(aBuilder)
  {
  }

  void
  Visit(nsPurpleBuffer& aBuffer, nsPurpleBufferEntry* aEntry)
  {
    MOZ_ASSERT(aEntry->mObject, "Null object in purple buffer");
    MOZ_ASSERT(aEntry->mRefCnt->get() != 0,
               "SelectPointersVisitor: snow-white object in the purple buffer");
    if (!aEntry->mRefCnt->IsPurple() ||
        AddPurpleRoot(mBuilder, aEntry->mObject, aEntry->mParticipant)) {
      aBuffer.Remove(aEntry);
    }
  }

private:
  CCGraphBuilder& mBuilder;
};

void
nsPurpleBuffer::SelectPointers(CCGraphBuilder& aBuilder)
{
  SelectPointersVisitor visitor(aBuilder);
  VisitEntries(visitor);

  NS_ASSERTION(mCount == 0, "AddPurpleRoot failed");
  if (mCount == 0) {
    FreeBlocks();
    InitBlocks();
  }
}

enum ccPhase
{
  IdlePhase,
  GraphBuildingPhase,
  ScanAndCollectWhitePhase,
  CleanupPhase
};

enum ccType
{
  SliceCC,     /* If a CC is in progress, continue it. Otherwise, start a new one. */
  ManualCC,    /* Explicitly triggered. */
  ShutdownCC   /* Shutdown CC, used for finding leaks. */
};

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

////////////////////////////////////////////////////////////////////////
// Top level structure for the cycle collector.
////////////////////////////////////////////////////////////////////////

using js::SliceBudget;

class JSPurpleBuffer;

class nsCycleCollector : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

private:
  bool mActivelyCollecting;
  bool mFreeingSnowWhite;
  // mScanInProgress should be false when we're collecting white objects.
  bool mScanInProgress;
  CycleCollectorResults mResults;
  TimeStamp mCollectionStart;

  CycleCollectedJSRuntime* mJSRuntime;

  ccPhase mIncrementalPhase;
  CCGraph mGraph;
  nsAutoPtr<CCGraphBuilder> mBuilder;
  RefPtr<nsCycleCollectorLogger> mLogger;

  DebugOnly<void*> mThread;

  nsCycleCollectorParams mParams;

  uint32_t mWhiteNodeCount;

  CC_BeforeUnlinkCallback mBeforeUnlinkCB;
  CC_ForgetSkippableCallback mForgetSkippableCB;

  nsPurpleBuffer mPurpleBuf;

  uint32_t mUnmergedNeeded;
  uint32_t mMergedInARow;

  RefPtr<JSPurpleBuffer> mJSPurpleBuffer;

private:
  virtual ~nsCycleCollector();

public:
  nsCycleCollector();

  void RegisterJSRuntime(CycleCollectedJSRuntime* aJSRuntime);
  void ForgetJSRuntime();

  void SetBeforeUnlinkCallback(CC_BeforeUnlinkCallback aBeforeUnlinkCB)
  {
    CheckThreadSafety();
    mBeforeUnlinkCB = aBeforeUnlinkCB;
  }

  void SetForgetSkippableCallback(CC_ForgetSkippableCallback aForgetSkippableCB)
  {
    CheckThreadSafety();
    mForgetSkippableCB = aForgetSkippableCB;
  }

  void Suspect(void* aPtr, nsCycleCollectionParticipant* aCp,
               nsCycleCollectingAutoRefCnt* aRefCnt);
  uint32_t SuspectedCount();
  void ForgetSkippable(bool aRemoveChildlessNodes, bool aAsyncSnowWhiteFreeing);
  bool FreeSnowWhite(bool aUntilNoSWInPurpleBuffer);

  // This method assumes its argument is already canonicalized.
  void RemoveObjectFromGraph(void* aPtr);

  void PrepareForGarbageCollection();
  void FinishAnyCurrentCollection();

  bool Collect(ccType aCCType,
               SliceBudget& aBudget,
               nsICycleCollectorListener* aManualListener,
               bool aPreferShorterSlices = false);
  void Shutdown();

  bool IsIdle() const { return mIncrementalPhase == IdlePhase; }

  void SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                           size_t* aObjectSize,
                           size_t* aGraphSize,
                           size_t* aPurpleBufferSize) const;

  JSPurpleBuffer* GetJSPurpleBuffer();
private:
  void CheckThreadSafety();
  void ShutdownCollect();

  void FixGrayBits(bool aForceGC, TimeLog& aTimeLog);
  bool IsIncrementalGCInProgress();
  void FinishAnyIncrementalGCInProgress();
  bool ShouldMergeZones(ccType aCCType);

  void BeginCollection(ccType aCCType, nsICycleCollectorListener* aManualListener);
  void MarkRoots(SliceBudget& aBudget);
  void ScanRoots(bool aFullySynchGraphBuild);
  void ScanIncrementalRoots();
  void ScanWhiteNodes(bool aFullySynchGraphBuild);
  void ScanBlackNodes();
  void ScanWeakMaps();

  // returns whether anything was collected
  bool CollectWhite();

  void CleanupAfterCollection();
};

NS_IMPL_ISUPPORTS(nsCycleCollector, nsIMemoryReporter)

/**
 * GraphWalker is templatized over a Visitor class that must provide
 * the following two methods:
 *
 * bool ShouldVisitNode(PtrInfo const *pi);
 * void VisitNode(PtrInfo *pi);
 */
template<class Visitor>
class GraphWalker
{
private:
  Visitor mVisitor;

  void DoWalk(nsDeque& aQueue);

  void CheckedPush(nsDeque& aQueue, PtrInfo* aPi)
  {
    if (!aPi) {
      MOZ_CRASH();
    }
    if (!aQueue.Push(aPi, fallible)) {
      mVisitor.Failed();
    }
  }

public:
  void Walk(PtrInfo* aPi);
  void WalkFromRoots(CCGraph& aGraph);
  // copy-constructing the visitor should be cheap, and less
  // indirection than using a reference
  explicit GraphWalker(const Visitor aVisitor) : mVisitor(aVisitor)
  {
  }
};


////////////////////////////////////////////////////////////////////////
// The static collector struct
////////////////////////////////////////////////////////////////////////

struct CollectorData
{
  RefPtr<nsCycleCollector> mCollector;
  CycleCollectedJSRuntime* mRuntime;
};

static mozilla::ThreadLocal<CollectorData*> sCollectorData;

////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////

static inline void
ToParticipant(nsISupports* aPtr, nsXPCOMCycleCollectionParticipant** aCp)
{
  // We use QI to move from an nsISupports to an
  // nsXPCOMCycleCollectionParticipant, which is a per-class singleton helper
  // object that implements traversal and unlinking logic for the nsISupports
  // in question.
  *aCp = nullptr;
  CallQueryInterface(aPtr, aCp);
}

template<class Visitor>
MOZ_NEVER_INLINE void
GraphWalker<Visitor>::Walk(PtrInfo* aPi)
{
  nsDeque queue;
  CheckedPush(queue, aPi);
  DoWalk(queue);
}

template<class Visitor>
MOZ_NEVER_INLINE void
GraphWalker<Visitor>::WalkFromRoots(CCGraph& aGraph)
{
  nsDeque queue;
  NodePool::Enumerator etor(aGraph.mNodes);
  for (uint32_t i = 0; i < aGraph.mRootCount; ++i) {
    CheckedPush(queue, etor.GetNext());
  }
  DoWalk(queue);
}

template<class Visitor>
MOZ_NEVER_INLINE void
GraphWalker<Visitor>::DoWalk(nsDeque& aQueue)
{
  // Use a aQueue to match the breadth-first traversal used when we
  // built the graph, for hopefully-better locality.
  while (aQueue.GetSize() > 0) {
    PtrInfo* pi = static_cast<PtrInfo*>(aQueue.PopFront());

    if (pi->WasTraversed() && mVisitor.ShouldVisitNode(pi)) {
      mVisitor.VisitNode(pi);
      for (EdgePool::Iterator child = pi->FirstChild(),
           child_end = pi->LastChild();
           child != child_end; ++child) {
        CheckedPush(aQueue, *child);
      }
    }
  }
}

struct CCGraphDescriber : public LinkedListElement<CCGraphDescriber>
{
  CCGraphDescriber()
    : mAddress("0x"), mCnt(0), mType(eUnknown)
  {
  }

  enum Type
  {
    eRefCountedObject,
    eGCedObject,
    eGCMarkedObject,
    eEdge,
    eRoot,
    eGarbage,
    eUnknown
  };

  nsCString mAddress;
  nsCString mName;
  nsCString mCompartmentOrToAddress;
  uint32_t mCnt;
  Type mType;
};

class nsCycleCollectorLogSinkToFile final : public nsICycleCollectorLogSink
{
public:
  NS_DECL_ISUPPORTS

  nsCycleCollectorLogSinkToFile() :
    mProcessIdentifier(base::GetCurrentProcId()),
    mGCLog("gc-edges"), mCCLog("cc-edges")
  {
  }

  NS_IMETHOD GetFilenameIdentifier(nsAString& aIdentifier) override
  {
    aIdentifier = mFilenameIdentifier;
    return NS_OK;
  }

  NS_IMETHOD SetFilenameIdentifier(const nsAString& aIdentifier) override
  {
    mFilenameIdentifier = aIdentifier;
    return NS_OK;
  }

  NS_IMETHOD GetProcessIdentifier(int32_t* aIdentifier) override
  {
    *aIdentifier = mProcessIdentifier;
    return NS_OK;
  }

  NS_IMETHOD SetProcessIdentifier(int32_t aIdentifier) override
  {
    mProcessIdentifier = aIdentifier;
    return NS_OK;
  }

  NS_IMETHOD GetGcLog(nsIFile** aPath) override
  {
    NS_IF_ADDREF(*aPath = mGCLog.mFile);
    return NS_OK;
  }

  NS_IMETHOD GetCcLog(nsIFile** aPath) override
  {
    NS_IF_ADDREF(*aPath = mCCLog.mFile);
    return NS_OK;
  }

  NS_IMETHOD Open(FILE** aGCLog, FILE** aCCLog) override
  {
    nsresult rv;

    if (mGCLog.mStream || mCCLog.mStream) {
      return NS_ERROR_UNEXPECTED;
    }

    rv = OpenLog(&mGCLog);
    NS_ENSURE_SUCCESS(rv, rv);
    *aGCLog = mGCLog.mStream;

    rv = OpenLog(&mCCLog);
    NS_ENSURE_SUCCESS(rv, rv);
    *aCCLog = mCCLog.mStream;

    return NS_OK;
  }

  NS_IMETHOD CloseGCLog() override
  {
    if (!mGCLog.mStream) {
      return NS_ERROR_UNEXPECTED;
    }
    CloseLog(&mGCLog, NS_LITERAL_STRING("Garbage"));
    return NS_OK;
  }

  NS_IMETHOD CloseCCLog() override
  {
    if (!mCCLog.mStream) {
      return NS_ERROR_UNEXPECTED;
    }
    CloseLog(&mCCLog, NS_LITERAL_STRING("Cycle"));
    return NS_OK;
  }

private:
  ~nsCycleCollectorLogSinkToFile()
  {
    if (mGCLog.mStream) {
      MozillaUnRegisterDebugFILE(mGCLog.mStream);
      fclose(mGCLog.mStream);
    }
    if (mCCLog.mStream) {
      MozillaUnRegisterDebugFILE(mCCLog.mStream);
      fclose(mCCLog.mStream);
    }
  }

  struct FileInfo
  {
    const char* const mPrefix;
    nsCOMPtr<nsIFile> mFile;
    FILE* mStream;

    explicit FileInfo(const char* aPrefix) : mPrefix(aPrefix), mStream(nullptr) { }
  };

  /**
   * Create a new file named something like aPrefix.$PID.$IDENTIFIER.log in
   * $MOZ_CC_LOG_DIRECTORY or in the system's temp directory.  No existing
   * file will be overwritten; if aPrefix.$PID.$IDENTIFIER.log exists, we'll
   * try a file named something like aPrefix.$PID.$IDENTIFIER-1.log, and so
   * on.
   */
  already_AddRefed<nsIFile> CreateTempFile(const char* aPrefix)
  {
    nsPrintfCString filename("%s.%d%s%s.log",
                             aPrefix,
                             mProcessIdentifier,
                             mFilenameIdentifier.IsEmpty() ? "" : ".",
                             NS_ConvertUTF16toUTF8(mFilenameIdentifier).get());

    // Get the log directory either from $MOZ_CC_LOG_DIRECTORY or from
    // the fallback directories in OpenTempFile.  We don't use an nsCOMPtr
    // here because OpenTempFile uses an in/out param and getter_AddRefs
    // wouldn't work.
    nsIFile* logFile = nullptr;
    if (char* env = PR_GetEnv("MOZ_CC_LOG_DIRECTORY")) {
      NS_NewNativeLocalFile(nsCString(env), /* followLinks = */ true,
                            &logFile);
    }

    // On Android or B2G, this function will open a file named
    // aFilename under a memory-reporting-specific folder
    // (/data/local/tmp/memory-reports). Otherwise, it will open a
    // file named aFilename under "NS_OS_TEMP_DIR".
    nsresult rv = nsDumpUtils::OpenTempFile(filename, &logFile,
                                            NS_LITERAL_CSTRING("memory-reports"));
    if (NS_FAILED(rv)) {
      NS_IF_RELEASE(logFile);
      return nullptr;
    }

    return dont_AddRef(logFile);
  }

  nsresult OpenLog(FileInfo* aLog)
  {
    // Initially create the log in a file starting with "incomplete-".
    // We'll move the file and strip off the "incomplete-" once the dump
    // completes.  (We do this because we don't want scripts which poll
    // the filesystem looking for GC/CC dumps to grab a file before we're
    // finished writing to it.)
    nsAutoCString incomplete;
    incomplete += "incomplete-";
    incomplete += aLog->mPrefix;
    MOZ_ASSERT(!aLog->mFile);
    aLog->mFile = CreateTempFile(incomplete.get());
    if (NS_WARN_IF(!aLog->mFile)) {
      return NS_ERROR_UNEXPECTED;
    }

    MOZ_ASSERT(!aLog->mStream);
    aLog->mFile->OpenANSIFileDesc("w", &aLog->mStream);
    if (NS_WARN_IF(!aLog->mStream)) {
      return NS_ERROR_UNEXPECTED;
    }
    MozillaRegisterDebugFILE(aLog->mStream);
    return NS_OK;
  }

  nsresult CloseLog(FileInfo* aLog, const nsAString& aCollectorKind)
  {
    MOZ_ASSERT(aLog->mStream);
    MOZ_ASSERT(aLog->mFile);

    MozillaUnRegisterDebugFILE(aLog->mStream);
    fclose(aLog->mStream);
    aLog->mStream = nullptr;

    // Strip off "incomplete-".
    nsCOMPtr<nsIFile> logFileFinalDestination =
      CreateTempFile(aLog->mPrefix);
    if (NS_WARN_IF(!logFileFinalDestination)) {
      return NS_ERROR_UNEXPECTED;
    }

    nsAutoString logFileFinalDestinationName;
    logFileFinalDestination->GetLeafName(logFileFinalDestinationName);
    if (NS_WARN_IF(logFileFinalDestinationName.IsEmpty())) {
      return NS_ERROR_UNEXPECTED;
    }

    aLog->mFile->MoveTo(/* directory */ nullptr, logFileFinalDestinationName);

    // Save the file path.
    aLog->mFile = logFileFinalDestination;

    // Log to the error console.
    nsCOMPtr<nsIConsoleService> cs =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      // Copy out the path.
      nsAutoString logPath;
      logFileFinalDestination->GetPath(logPath);

      nsString msg = aCollectorKind +
        NS_LITERAL_STRING(" Collector log dumped to ") + logPath;
      cs->LogStringMessage(msg.get());
    }
    return NS_OK;
  }

  int32_t mProcessIdentifier;
  nsString mFilenameIdentifier;
  FileInfo mGCLog;
  FileInfo mCCLog;
};

NS_IMPL_ISUPPORTS(nsCycleCollectorLogSinkToFile, nsICycleCollectorLogSink)


class nsCycleCollectorLogger final : public nsICycleCollectorListener
{
  ~nsCycleCollectorLogger()
  {
    ClearDescribers();
  }

public:
  nsCycleCollectorLogger()
    : mLogSink(nsCycleCollector_createLogSink())
    , mWantAllTraces(false)
    , mDisableLog(false)
    , mWantAfterProcessing(false)
    , mCCLog(nullptr)
  {
  }

  NS_DECL_ISUPPORTS

  void SetAllTraces()
  {
    mWantAllTraces = true;
  }

  bool IsAllTraces()
  {
    return mWantAllTraces;
  }

  NS_IMETHOD AllTraces(nsICycleCollectorListener** aListener) override
  {
    SetAllTraces();
    NS_ADDREF(*aListener = this);
    return NS_OK;
  }

  NS_IMETHOD GetWantAllTraces(bool* aAllTraces) override
  {
    *aAllTraces = mWantAllTraces;
    return NS_OK;
  }

  NS_IMETHOD GetDisableLog(bool* aDisableLog) override
  {
    *aDisableLog = mDisableLog;
    return NS_OK;
  }

  NS_IMETHOD SetDisableLog(bool aDisableLog) override
  {
    mDisableLog = aDisableLog;
    return NS_OK;
  }

  NS_IMETHOD GetWantAfterProcessing(bool* aWantAfterProcessing) override
  {
    *aWantAfterProcessing = mWantAfterProcessing;
    return NS_OK;
  }

  NS_IMETHOD SetWantAfterProcessing(bool aWantAfterProcessing) override
  {
    mWantAfterProcessing = aWantAfterProcessing;
    return NS_OK;
  }

  NS_IMETHOD GetLogSink(nsICycleCollectorLogSink** aLogSink) override
  {
    NS_ADDREF(*aLogSink = mLogSink);
    return NS_OK;
  }

  NS_IMETHOD SetLogSink(nsICycleCollectorLogSink* aLogSink) override
  {
    if (!aLogSink) {
      return NS_ERROR_INVALID_ARG;
    }
    mLogSink = aLogSink;
    return NS_OK;
  }

  nsresult Begin()
  {
    nsresult rv;

    mCurrentAddress.AssignLiteral("0x");
    ClearDescribers();
    if (mDisableLog) {
      return NS_OK;
    }

    FILE* gcLog;
    rv = mLogSink->Open(&gcLog, &mCCLog);
    NS_ENSURE_SUCCESS(rv, rv);
    // Dump the JS heap.
    CollectorData* data = sCollectorData.get();
    if (data && data->mRuntime) {
      data->mRuntime->DumpJSHeap(gcLog);
    }
    rv = mLogSink->CloseGCLog();
    NS_ENSURE_SUCCESS(rv, rv);

    fprintf(mCCLog, "# WantAllTraces=%s\n", mWantAllTraces ? "true" : "false");
    return NS_OK;
  }
  void NoteRefCountedObject(uint64_t aAddress, uint32_t aRefCount,
                            const char* aObjectDescription)
  {
    if (!mDisableLog) {
      fprintf(mCCLog, "%p [rc=%u] %s\n", (void*)aAddress, aRefCount,
              aObjectDescription);
    }
    if (mWantAfterProcessing) {
      CCGraphDescriber* d =  new CCGraphDescriber();
      mDescribers.insertBack(d);
      mCurrentAddress.AssignLiteral("0x");
      mCurrentAddress.AppendInt(aAddress, 16);
      d->mType = CCGraphDescriber::eRefCountedObject;
      d->mAddress = mCurrentAddress;
      d->mCnt = aRefCount;
      d->mName.Append(aObjectDescription);
    }
  }
  void NoteGCedObject(uint64_t aAddress, bool aMarked,
                      const char* aObjectDescription,
                      uint64_t aCompartmentAddress)
  {
    if (!mDisableLog) {
      fprintf(mCCLog, "%p [gc%s] %s\n", (void*)aAddress,
              aMarked ? ".marked" : "", aObjectDescription);
    }
    if (mWantAfterProcessing) {
      CCGraphDescriber* d =  new CCGraphDescriber();
      mDescribers.insertBack(d);
      mCurrentAddress.AssignLiteral("0x");
      mCurrentAddress.AppendInt(aAddress, 16);
      d->mType = aMarked ? CCGraphDescriber::eGCMarkedObject :
                           CCGraphDescriber::eGCedObject;
      d->mAddress = mCurrentAddress;
      d->mName.Append(aObjectDescription);
      if (aCompartmentAddress) {
        d->mCompartmentOrToAddress.AssignLiteral("0x");
        d->mCompartmentOrToAddress.AppendInt(aCompartmentAddress, 16);
      } else {
        d->mCompartmentOrToAddress.SetIsVoid(true);
      }
    }
  }
  void NoteEdge(uint64_t aToAddress, const char* aEdgeName)
  {
    if (!mDisableLog) {
      fprintf(mCCLog, "> %p %s\n", (void*)aToAddress, aEdgeName);
    }
    if (mWantAfterProcessing) {
      CCGraphDescriber* d =  new CCGraphDescriber();
      mDescribers.insertBack(d);
      d->mType = CCGraphDescriber::eEdge;
      d->mAddress = mCurrentAddress;
      d->mCompartmentOrToAddress.AssignLiteral("0x");
      d->mCompartmentOrToAddress.AppendInt(aToAddress, 16);
      d->mName.Append(aEdgeName);
    }
  }
  void NoteWeakMapEntry(uint64_t aMap, uint64_t aKey,
                        uint64_t aKeyDelegate, uint64_t aValue)
  {
    if (!mDisableLog) {
      fprintf(mCCLog, "WeakMapEntry map=%p key=%p keyDelegate=%p value=%p\n",
              (void*)aMap, (void*)aKey, (void*)aKeyDelegate, (void*)aValue);
    }
    // We don't support after-processing for weak map entries.
  }
  void NoteIncrementalRoot(uint64_t aAddress)
  {
    if (!mDisableLog) {
      fprintf(mCCLog, "IncrementalRoot %p\n", (void*)aAddress);
    }
    // We don't support after-processing for incremental roots.
  }
  void BeginResults()
  {
    if (!mDisableLog) {
      fputs("==========\n", mCCLog);
    }
  }
  void DescribeRoot(uint64_t aAddress, uint32_t aKnownEdges)
  {
    if (!mDisableLog) {
      fprintf(mCCLog, "%p [known=%u]\n", (void*)aAddress, aKnownEdges);
    }
    if (mWantAfterProcessing) {
      CCGraphDescriber* d =  new CCGraphDescriber();
      mDescribers.insertBack(d);
      d->mType = CCGraphDescriber::eRoot;
      d->mAddress.AppendInt(aAddress, 16);
      d->mCnt = aKnownEdges;
    }
  }
  void DescribeGarbage(uint64_t aAddress)
  {
    if (!mDisableLog) {
      fprintf(mCCLog, "%p [garbage]\n", (void*)aAddress);
    }
    if (mWantAfterProcessing) {
      CCGraphDescriber* d =  new CCGraphDescriber();
      mDescribers.insertBack(d);
      d->mType = CCGraphDescriber::eGarbage;
      d->mAddress.AppendInt(aAddress, 16);
    }
  }
  void End()
  {
    if (!mDisableLog) {
      mCCLog = nullptr;
      NS_WARN_IF(NS_FAILED(mLogSink->CloseCCLog()));
    }
  }
  NS_IMETHOD ProcessNext(nsICycleCollectorHandler* aHandler,
                         bool* aCanContinue) override
  {
    if (NS_WARN_IF(!aHandler) || NS_WARN_IF(!mWantAfterProcessing)) {
      return NS_ERROR_UNEXPECTED;
    }
    CCGraphDescriber* d = mDescribers.popFirst();
    if (d) {
      switch (d->mType) {
        case CCGraphDescriber::eRefCountedObject:
          aHandler->NoteRefCountedObject(d->mAddress,
                                         d->mCnt,
                                         d->mName);
          break;
        case CCGraphDescriber::eGCedObject:
        case CCGraphDescriber::eGCMarkedObject:
          aHandler->NoteGCedObject(d->mAddress,
                                   d->mType ==
                                     CCGraphDescriber::eGCMarkedObject,
                                   d->mName,
                                   d->mCompartmentOrToAddress);
          break;
        case CCGraphDescriber::eEdge:
          aHandler->NoteEdge(d->mAddress,
                             d->mCompartmentOrToAddress,
                             d->mName);
          break;
        case CCGraphDescriber::eRoot:
          aHandler->DescribeRoot(d->mAddress,
                                 d->mCnt);
          break;
        case CCGraphDescriber::eGarbage:
          aHandler->DescribeGarbage(d->mAddress);
          break;
        case CCGraphDescriber::eUnknown:
          NS_NOTREACHED("CCGraphDescriber::eUnknown");
          break;
      }
      delete d;
    }
    if (!(*aCanContinue = !mDescribers.isEmpty())) {
      mCurrentAddress.AssignLiteral("0x");
    }
    return NS_OK;
  }
  NS_IMETHOD AsLogger(nsCycleCollectorLogger** aRetVal) override
  {
    RefPtr<nsCycleCollectorLogger> rval = this;
    rval.forget(aRetVal);
    return NS_OK;
  }
private:
  void ClearDescribers()
  {
    CCGraphDescriber* d;
    while ((d = mDescribers.popFirst())) {
      delete d;
    }
  }

  nsCOMPtr<nsICycleCollectorLogSink> mLogSink;
  bool mWantAllTraces;
  bool mDisableLog;
  bool mWantAfterProcessing;
  nsCString mCurrentAddress;
  mozilla::LinkedList<CCGraphDescriber> mDescribers;
  FILE* mCCLog;
};

NS_IMPL_ISUPPORTS(nsCycleCollectorLogger, nsICycleCollectorListener)

nsresult
nsCycleCollectorLoggerConstructor(nsISupports* aOuter,
                                  const nsIID& aIID,
                                  void** aInstancePtr)
{
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsISupports* logger = new nsCycleCollectorLogger();

  return logger->QueryInterface(aIID, aInstancePtr);
}

static bool
GCThingIsGrayCCThing(JS::GCCellPtr thing)
{
    return AddToCCKind(thing.kind()) &&
           JS::GCThingIsMarkedGray(thing);
}

static bool
ValueIsGrayCCThing(const JS::Value& value)
{
    return AddToCCKind(value.traceKind()) &&
           JS::GCThingIsMarkedGray(value.toGCCellPtr());
}

////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |MarkRoots| routine.
////////////////////////////////////////////////////////////////////////

class CCGraphBuilder final : public nsCycleCollectionTraversalCallback,
  public nsCycleCollectionNoteRootCallback
{
private:
  CCGraph& mGraph;
  CycleCollectorResults& mResults;
  NodePool::Builder mNodeBuilder;
  EdgePool::Builder mEdgeBuilder;
  PtrInfo* mCurrPi;
  nsCycleCollectionParticipant* mJSParticipant;
  nsCycleCollectionParticipant* mJSZoneParticipant;
  nsCString mNextEdgeName;
  RefPtr<nsCycleCollectorLogger> mLogger;
  bool mMergeZones;
  nsAutoPtr<NodePool::Enumerator> mCurrNode;

public:
  CCGraphBuilder(CCGraph& aGraph,
                 CycleCollectorResults& aResults,
                 CycleCollectedJSRuntime* aJSRuntime,
                 nsCycleCollectorLogger* aLogger,
                 bool aMergeZones);
  virtual ~CCGraphBuilder();

  bool WantAllTraces() const
  {
    return nsCycleCollectionNoteRootCallback::WantAllTraces();
  }

  bool AddPurpleRoot(void* aRoot, nsCycleCollectionParticipant* aParti);

  // This is called when all roots have been added to the graph, to prepare for BuildGraph().
  void DoneAddingRoots();

  // Do some work traversing nodes in the graph. Returns true if this graph building is finished.
  bool BuildGraph(SliceBudget& aBudget);

private:
  PtrInfo* AddNode(void* aPtr, nsCycleCollectionParticipant* aParticipant);
  PtrInfo* AddWeakMapNode(JS::GCCellPtr aThing);
  PtrInfo* AddWeakMapNode(JSObject* aObject);

  void SetFirstChild()
  {
    mCurrPi->SetFirstChild(mEdgeBuilder.Mark());
  }

  void SetLastChild()
  {
    mCurrPi->SetLastChild(mEdgeBuilder.Mark());
  }

public:
  // nsCycleCollectionNoteRootCallback methods.
  NS_IMETHOD_(void) NoteXPCOMRoot(nsISupports* aRoot);
  NS_IMETHOD_(void) NoteJSRoot(JSObject* aRoot);
  NS_IMETHOD_(void) NoteNativeRoot(void* aRoot,
                                   nsCycleCollectionParticipant* aParticipant);
  NS_IMETHOD_(void) NoteWeakMapping(JSObject* aMap, JS::GCCellPtr aKey,
                                    JSObject* aKdelegate, JS::GCCellPtr aVal);

  // nsCycleCollectionTraversalCallback methods.
  NS_IMETHOD_(void) DescribeRefCountedNode(nsrefcnt aRefCount,
                                           const char* aObjName);
  NS_IMETHOD_(void) DescribeGCedNode(bool aIsMarked, const char* aObjName,
                                     uint64_t aCompartmentAddress);

  NS_IMETHOD_(void) NoteXPCOMChild(nsISupports* aChild);
  NS_IMETHOD_(void) NoteJSObject(JSObject* aChild);
  NS_IMETHOD_(void) NoteJSScript(JSScript* aChild);
  NS_IMETHOD_(void) NoteNativeChild(void* aChild,
                                    nsCycleCollectionParticipant* aParticipant);
  NS_IMETHOD_(void) NoteNextEdgeName(const char* aName);

private:
  void NoteJSChild(JS::GCCellPtr aChild);

  NS_IMETHOD_(void) NoteRoot(void* aRoot,
                             nsCycleCollectionParticipant* aParticipant)
  {
    MOZ_ASSERT(aRoot);
    MOZ_ASSERT(aParticipant);

    if (!aParticipant->CanSkipInCC(aRoot) || MOZ_UNLIKELY(WantAllTraces())) {
      AddNode(aRoot, aParticipant);
    }
  }

  NS_IMETHOD_(void) NoteChild(void* aChild, nsCycleCollectionParticipant* aCp,
                              nsCString& aEdgeName)
  {
    PtrInfo* childPi = AddNode(aChild, aCp);
    if (!childPi) {
      return;
    }
    mEdgeBuilder.Add(childPi);
    if (mLogger) {
      mLogger->NoteEdge((uint64_t)aChild, aEdgeName.get());
    }
    ++childPi->mInternalRefs;
  }

  JS::Zone* MergeZone(JS::GCCellPtr aGcthing)
  {
    if (!mMergeZones) {
      return nullptr;
    }
    JS::Zone* zone = JS::GetTenuredGCThingZone(aGcthing);
    if (js::IsSystemZone(zone)) {
      return nullptr;
    }
    return zone;
  }
};

CCGraphBuilder::CCGraphBuilder(CCGraph& aGraph,
                               CycleCollectorResults& aResults,
                               CycleCollectedJSRuntime* aJSRuntime,
                               nsCycleCollectorLogger* aLogger,
                               bool aMergeZones)
  : mGraph(aGraph)
  , mResults(aResults)
  , mNodeBuilder(aGraph.mNodes)
  , mEdgeBuilder(aGraph.mEdges)
  , mJSParticipant(nullptr)
  , mJSZoneParticipant(nullptr)
  , mLogger(aLogger)
  , mMergeZones(aMergeZones)
{
  if (aJSRuntime) {
    mJSParticipant = aJSRuntime->GCThingParticipant();
    mJSZoneParticipant = aJSRuntime->ZoneParticipant();
  }

  if (mLogger) {
    mFlags |= nsCycleCollectionTraversalCallback::WANT_DEBUG_INFO;
    if (mLogger->IsAllTraces()) {
      mFlags |= nsCycleCollectionTraversalCallback::WANT_ALL_TRACES;
      mWantAllTraces = true; // for nsCycleCollectionNoteRootCallback
    }
  }

  mMergeZones = mMergeZones && MOZ_LIKELY(!WantAllTraces());

  MOZ_ASSERT(nsCycleCollectionNoteRootCallback::WantAllTraces() ==
             nsCycleCollectionTraversalCallback::WantAllTraces());
}

CCGraphBuilder::~CCGraphBuilder()
{
}

PtrInfo*
CCGraphBuilder::AddNode(void* aPtr, nsCycleCollectionParticipant* aParticipant)
{
  PtrToNodeEntry* e = mGraph.AddNodeToMap(aPtr);
  if (!e) {
    return nullptr;
  }

  PtrInfo* result;
  if (!e->mNode) {
    // New entry.
    result = mNodeBuilder.Add(aPtr, aParticipant);
    e->mNode = result;
    NS_ASSERTION(result, "mNodeBuilder.Add returned null");
  } else {
    result = e->mNode;
    MOZ_ASSERT(result->mParticipant == aParticipant,
               "nsCycleCollectionParticipant shouldn't change!");
  }
  return result;
}

bool
CCGraphBuilder::AddPurpleRoot(void* aRoot, nsCycleCollectionParticipant* aParti)
{
  CanonicalizeParticipant(&aRoot, &aParti);

  if (WantAllTraces() || !aParti->CanSkipInCC(aRoot)) {
    PtrInfo* pinfo = AddNode(aRoot, aParti);
    if (!pinfo) {
      return false;
    }
  }

  return true;
}

void
CCGraphBuilder::DoneAddingRoots()
{
  // We've finished adding roots, and everything in the graph is a root.
  mGraph.mRootCount = mGraph.MapCount();

  mCurrNode = new NodePool::Enumerator(mGraph.mNodes);
}

MOZ_NEVER_INLINE bool
CCGraphBuilder::BuildGraph(SliceBudget& aBudget)
{
  const intptr_t kNumNodesBetweenTimeChecks = 1000;
  const intptr_t kStep = SliceBudget::CounterReset / kNumNodesBetweenTimeChecks;

  MOZ_ASSERT(mCurrNode);

  while (!aBudget.isOverBudget() && !mCurrNode->IsDone()) {
    PtrInfo* pi = mCurrNode->GetNext();
    if (!pi) {
      MOZ_CRASH();
    }

    mCurrPi = pi;

    // We need to call SetFirstChild() even on deleted nodes, to set their
    // firstChild() that may be read by a prior non-deleted neighbor.
    SetFirstChild();

    if (pi->mParticipant) {
      nsresult rv = pi->mParticipant->Traverse(pi->mPointer, *this);
      MOZ_RELEASE_ASSERT(!NS_FAILED(rv), "Cycle collector Traverse method failed");
    }

    if (mCurrNode->AtBlockEnd()) {
      SetLastChild();
    }

    aBudget.step(kStep);
  }

  if (!mCurrNode->IsDone()) {
    return false;
  }

  if (mGraph.mRootCount > 0) {
    SetLastChild();
  }

  mCurrNode = nullptr;

  return true;
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteXPCOMRoot(nsISupports* aRoot)
{
  aRoot = CanonicalizeXPCOMParticipant(aRoot);
  NS_ASSERTION(aRoot,
               "Don't add objects that don't participate in collection!");

  nsXPCOMCycleCollectionParticipant* cp;
  ToParticipant(aRoot, &cp);

  NoteRoot(aRoot, cp);
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteJSRoot(JSObject* aRoot)
{
  if (JS::Zone* zone = MergeZone(JS::GCCellPtr(aRoot))) {
    NoteRoot(zone, mJSZoneParticipant);
  } else {
    NoteRoot(aRoot, mJSParticipant);
  }
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteNativeRoot(void* aRoot,
                               nsCycleCollectionParticipant* aParticipant)
{
  NoteRoot(aRoot, aParticipant);
}

NS_IMETHODIMP_(void)
CCGraphBuilder::DescribeRefCountedNode(nsrefcnt aRefCount, const char* aObjName)
{
  MOZ_RELEASE_ASSERT(aRefCount != 0, "CCed refcounted object has zero refcount");
  MOZ_RELEASE_ASSERT(aRefCount != UINT32_MAX, "CCed refcounted object has overflowing refcount");

  mResults.mVisitedRefCounted++;

  if (mLogger) {
    mLogger->NoteRefCountedObject((uint64_t)mCurrPi->mPointer, aRefCount,
                                  aObjName);
  }

  mCurrPi->mRefCount = aRefCount;
}

NS_IMETHODIMP_(void)
CCGraphBuilder::DescribeGCedNode(bool aIsMarked, const char* aObjName,
                                 uint64_t aCompartmentAddress)
{
  uint32_t refCount = aIsMarked ? UINT32_MAX : 0;
  mResults.mVisitedGCed++;

  if (mLogger) {
    mLogger->NoteGCedObject((uint64_t)mCurrPi->mPointer, aIsMarked,
                            aObjName, aCompartmentAddress);
  }

  mCurrPi->mRefCount = refCount;
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteXPCOMChild(nsISupports* aChild)
{
  nsCString edgeName;
  if (WantDebugInfo()) {
    edgeName.Assign(mNextEdgeName);
    mNextEdgeName.Truncate();
  }
  if (!aChild || !(aChild = CanonicalizeXPCOMParticipant(aChild))) {
    return;
  }

  nsXPCOMCycleCollectionParticipant* cp;
  ToParticipant(aChild, &cp);
  if (cp && (!cp->CanSkipThis(aChild) || WantAllTraces())) {
    NoteChild(aChild, cp, edgeName);
  }
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteNativeChild(void* aChild,
                                nsCycleCollectionParticipant* aParticipant)
{
  nsCString edgeName;
  if (WantDebugInfo()) {
    edgeName.Assign(mNextEdgeName);
    mNextEdgeName.Truncate();
  }
  if (!aChild) {
    return;
  }

  MOZ_ASSERT(aParticipant, "Need a nsCycleCollectionParticipant!");
  if (!aParticipant->CanSkipThis(aChild) || WantAllTraces()) {
    NoteChild(aChild, aParticipant, edgeName);
  }
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteJSObject(JSObject* aChild)
{
  return NoteJSChild(JS::GCCellPtr(aChild));
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteJSScript(JSScript* aChild)
{
  return NoteJSChild(JS::GCCellPtr(aChild));
}

void
CCGraphBuilder::NoteJSChild(JS::GCCellPtr aChild)
{
  if (!aChild) {
    return;
  }

  nsCString edgeName;
  if (MOZ_UNLIKELY(WantDebugInfo())) {
    edgeName.Assign(mNextEdgeName);
    mNextEdgeName.Truncate();
  }

  if (GCThingIsGrayCCThing(aChild) || MOZ_UNLIKELY(WantAllTraces())) {
    if (JS::Zone* zone = MergeZone(aChild)) {
      NoteChild(zone, mJSZoneParticipant, edgeName);
    } else {
      NoteChild(aChild.asCell(), mJSParticipant, edgeName);
    }
  }
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteNextEdgeName(const char* aName)
{
  if (WantDebugInfo()) {
    mNextEdgeName = aName;
  }
}

PtrInfo*
CCGraphBuilder::AddWeakMapNode(JS::GCCellPtr aNode)
{
  MOZ_ASSERT(aNode, "Weak map node should be non-null.");

  if (!GCThingIsGrayCCThing(aNode) && !WantAllTraces()) {
    return nullptr;
  }

  if (JS::Zone* zone = MergeZone(aNode)) {
    return AddNode(zone, mJSZoneParticipant);
  }
  return AddNode(aNode.asCell(), mJSParticipant);
}

PtrInfo*
CCGraphBuilder::AddWeakMapNode(JSObject* aObject)
{
  return AddWeakMapNode(JS::GCCellPtr(aObject));
}

NS_IMETHODIMP_(void)
CCGraphBuilder::NoteWeakMapping(JSObject* aMap, JS::GCCellPtr aKey,
                                JSObject* aKdelegate, JS::GCCellPtr aVal)
{
  // Don't try to optimize away the entry here, as we've already attempted to
  // do that in TraceWeakMapping in nsXPConnect.
  WeakMapping* mapping = mGraph.mWeakMaps.AppendElement();
  mapping->mMap = aMap ? AddWeakMapNode(aMap) : nullptr;
  mapping->mKey = aKey ? AddWeakMapNode(aKey) : nullptr;
  mapping->mKeyDelegate = aKdelegate ? AddWeakMapNode(aKdelegate) : mapping->mKey;
  mapping->mVal = aVal ? AddWeakMapNode(aVal) : nullptr;

  if (mLogger) {
    mLogger->NoteWeakMapEntry((uint64_t)aMap, aKey.unsafeAsInteger(),
                              (uint64_t)aKdelegate, aVal.unsafeAsInteger());
  }
}

static bool
AddPurpleRoot(CCGraphBuilder& aBuilder, void* aRoot,
              nsCycleCollectionParticipant* aParti)
{
  return aBuilder.AddPurpleRoot(aRoot, aParti);
}

// MayHaveChild() will be false after a Traverse if the object does
// not have any children the CC will visit.
class ChildFinder : public nsCycleCollectionTraversalCallback
{
public:
  ChildFinder() : mMayHaveChild(false)
  {
  }

  // The logic of the Note*Child functions must mirror that of their
  // respective functions in CCGraphBuilder.
  NS_IMETHOD_(void) NoteXPCOMChild(nsISupports* aChild);
  NS_IMETHOD_(void) NoteNativeChild(void* aChild,
                                    nsCycleCollectionParticipant* aHelper);
  NS_IMETHOD_(void) NoteJSObject(JSObject* aChild);
  NS_IMETHOD_(void) NoteJSScript(JSScript* aChild);

  NS_IMETHOD_(void) DescribeRefCountedNode(nsrefcnt aRefcount,
                                           const char* aObjname)
  {
  }
  NS_IMETHOD_(void) DescribeGCedNode(bool aIsMarked,
                                     const char* aObjname,
                                     uint64_t aCompartmentAddress)
  {
  }
  NS_IMETHOD_(void) NoteNextEdgeName(const char* aName)
  {
  }
  bool MayHaveChild()
  {
    return mMayHaveChild;
  }
private:
  bool mMayHaveChild;
};

NS_IMETHODIMP_(void)
ChildFinder::NoteXPCOMChild(nsISupports* aChild)
{
  if (!aChild || !(aChild = CanonicalizeXPCOMParticipant(aChild))) {
    return;
  }
  nsXPCOMCycleCollectionParticipant* cp;
  ToParticipant(aChild, &cp);
  if (cp && !cp->CanSkip(aChild, true)) {
    mMayHaveChild = true;
  }
}

NS_IMETHODIMP_(void)
ChildFinder::NoteNativeChild(void* aChild,
                             nsCycleCollectionParticipant* aHelper)
{
  if (!aChild) {
    return;
  }
  MOZ_ASSERT(aHelper, "Native child must have a participant");
  if (!aHelper->CanSkip(aChild, true)) {
    mMayHaveChild = true;
  }
}

NS_IMETHODIMP_(void)
ChildFinder::NoteJSObject(JSObject* aChild)
{
  if (aChild && JS::ObjectIsMarkedGray(aChild)) {
    mMayHaveChild = true;
  }
}

NS_IMETHODIMP_(void)
ChildFinder::NoteJSScript(JSScript* aChild)
{
  if (aChild && JS::ScriptIsMarkedGray(aChild)) {
    mMayHaveChild = true;
  }
}

static bool
MayHaveChild(void* aObj, nsCycleCollectionParticipant* aCp)
{
  ChildFinder cf;
  aCp->Traverse(aObj, cf);
  return cf.MayHaveChild();
}

// JSPurpleBuffer keeps references to GCThings which might affect the
// next cycle collection. It is owned only by itself and during unlink its
// self reference is broken down and the object ends up killing itself.
// If GC happens before CC, references to GCthings and the self reference are
// removed.
class JSPurpleBuffer
{
  ~JSPurpleBuffer()
  {
    MOZ_ASSERT(mValues.IsEmpty());
    MOZ_ASSERT(mObjects.IsEmpty());
  }

public:
  explicit JSPurpleBuffer(RefPtr<JSPurpleBuffer>& aReferenceToThis)
    : mReferenceToThis(aReferenceToThis)
    , mValues(kSegmentSize)
    , mObjects(kSegmentSize)
  {
    mReferenceToThis = this;
    mozilla::HoldJSObjects(this);
  }

  void Destroy()
  {
    mReferenceToThis = nullptr;
    mValues.Clear();
    mObjects.Clear();
    mozilla::DropJSObjects(this);
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(JSPurpleBuffer)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(JSPurpleBuffer)

  RefPtr<JSPurpleBuffer>& mReferenceToThis;

  // These are raw pointers instead of Heap<T> because we only need Heap<T> for
  // pointers which may point into the nursery. The purple buffer never contains
  // pointers to the nursery because nursery gcthings can never be gray and only
  // gray things can be inserted into the purple buffer.
  static const size_t kSegmentSize = 512;
  SegmentedVector<JS::Value, kSegmentSize, InfallibleAllocPolicy> mValues;
  SegmentedVector<JSObject*, kSegmentSize, InfallibleAllocPolicy> mObjects;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(JSPurpleBuffer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(JSPurpleBuffer)
  tmp->Destroy();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(JSPurpleBuffer)
  CycleCollectionNoteChild(cb, tmp, "self");
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_TRACE_SEGMENTED_ARRAY(_field, _type)                               \
  {                                                                           \
    for (auto iter = tmp->_field.Iter(); !iter.Done(); iter.Next()) {         \
      js::gc::CallTraceCallbackOnNonHeap<_type, TraceCallbacks>(              \
          &iter.Get(), aCallbacks, #_field, aClosure);                        \
    }                                                                         \
  }

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(JSPurpleBuffer)
  NS_TRACE_SEGMENTED_ARRAY(mValues, JS::Value)
  NS_TRACE_SEGMENTED_ARRAY(mObjects, JSObject*)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(JSPurpleBuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(JSPurpleBuffer, Release)

class SnowWhiteKiller : public TraceCallbacks
{
  struct SnowWhiteObject
  {
    void* mPointer;
    nsCycleCollectionParticipant* mParticipant;
    nsCycleCollectingAutoRefCnt* mRefCnt;
  };

  // Segments are 4 KiB on 32-bit and 8 KiB on 64-bit.
  static const size_t kSegmentSize = sizeof(void*) * 1024;
  typedef SegmentedVector<SnowWhiteObject, kSegmentSize, InfallibleAllocPolicy>
    ObjectsVector;

public:
  SnowWhiteKiller(nsCycleCollector* aCollector, uint32_t aMaxCount)
    : mCollector(aCollector)
    , mObjects(kSegmentSize)
  {
    MOZ_ASSERT(mCollector, "Calling SnowWhiteKiller after nsCC went away");
  }

  ~SnowWhiteKiller()
  {
    for (auto iter = mObjects.Iter(); !iter.Done(); iter.Next()) {
      SnowWhiteObject& o = iter.Get();
      if (!o.mRefCnt->get() && !o.mRefCnt->IsInPurpleBuffer()) {
        mCollector->RemoveObjectFromGraph(o.mPointer);
        o.mRefCnt->stabilizeForDeletion();
        o.mParticipant->Trace(o.mPointer, *this, nullptr);
        o.mParticipant->DeleteCycleCollectable(o.mPointer);
      }
    }
  }

  void
  Visit(nsPurpleBuffer& aBuffer, nsPurpleBufferEntry* aEntry)
  {
    MOZ_ASSERT(aEntry->mObject, "Null object in purple buffer");
    if (!aEntry->mRefCnt->get()) {
      void* o = aEntry->mObject;
      nsCycleCollectionParticipant* cp = aEntry->mParticipant;
      CanonicalizeParticipant(&o, &cp);
      SnowWhiteObject swo = { o, cp, aEntry->mRefCnt };
      mObjects.InfallibleAppend(swo);
      aBuffer.Remove(aEntry);
    }
  }

  bool HasSnowWhiteObjects() const
  {
    return !mObjects.IsEmpty();
  }

  virtual void Trace(JS::Heap<JS::Value>* aValue, const char* aName,
                     void* aClosure) const override
  {
    if (aValue->isMarkable() && ValueIsGrayCCThing(*aValue)) {
      MOZ_ASSERT(!js::gc::IsInsideNursery(aValue->toGCThing()));
      mCollector->GetJSPurpleBuffer()->mValues.InfallibleAppend(*aValue);
    }
  }

  virtual void Trace(JS::Heap<jsid>* aId, const char* aName,
                     void* aClosure) const override
  {
  }

  void AppendJSObjectToPurpleBuffer(JSObject* obj) const
  {
    if (obj && JS::ObjectIsMarkedGray(obj)) {
      MOZ_ASSERT(JS::ObjectIsTenured(obj));
      mCollector->GetJSPurpleBuffer()->mObjects.InfallibleAppend(obj);
    }
  }

  virtual void Trace(JS::Heap<JSObject*>* aObject, const char* aName,
                     void* aClosure) const override
  {
    AppendJSObjectToPurpleBuffer(*aObject);
  }

  virtual void Trace(JSObject** aObject, const char* aName,
                     void* aClosure) const override
  {
    AppendJSObjectToPurpleBuffer(*aObject);
  }

  virtual void Trace(JS::TenuredHeap<JSObject*>* aObject, const char* aName,
                     void* aClosure) const override
  {
    AppendJSObjectToPurpleBuffer(*aObject);
  }

  virtual void Trace(JS::Heap<JSString*>* aString, const char* aName,
                     void* aClosure) const override
  {
  }

  virtual void Trace(JS::Heap<JSScript*>* aScript, const char* aName,
                     void* aClosure) const override
  {
  }

  virtual void Trace(JS::Heap<JSFunction*>* aFunction, const char* aName,
                     void* aClosure) const override
  {
  }

private:
  RefPtr<nsCycleCollector> mCollector;
  ObjectsVector mObjects;
};

class RemoveSkippableVisitor : public SnowWhiteKiller
{
public:
  RemoveSkippableVisitor(nsCycleCollector* aCollector,
                         uint32_t aMaxCount, bool aRemoveChildlessNodes,
                         bool aAsyncSnowWhiteFreeing,
                         CC_ForgetSkippableCallback aCb)
    : SnowWhiteKiller(aCollector, aAsyncSnowWhiteFreeing ? 0 : aMaxCount)
    , mRemoveChildlessNodes(aRemoveChildlessNodes)
    , mAsyncSnowWhiteFreeing(aAsyncSnowWhiteFreeing)
    , mDispatchedDeferredDeletion(false)
    , mCallback(aCb)
  {
  }

  ~RemoveSkippableVisitor()
  {
    // Note, we must call the callback before SnowWhiteKiller calls
    // DeleteCycleCollectable!
    if (mCallback) {
      mCallback();
    }
    if (HasSnowWhiteObjects()) {
      // Effectively a continuation.
      nsCycleCollector_dispatchDeferredDeletion(true);
    }
  }

  void
  Visit(nsPurpleBuffer& aBuffer, nsPurpleBufferEntry* aEntry)
  {
    MOZ_ASSERT(aEntry->mObject, "null mObject in purple buffer");
    if (!aEntry->mRefCnt->get()) {
      if (!mAsyncSnowWhiteFreeing) {
        SnowWhiteKiller::Visit(aBuffer, aEntry);
      } else if (!mDispatchedDeferredDeletion) {
        mDispatchedDeferredDeletion = true;
        nsCycleCollector_dispatchDeferredDeletion(false);
      }
      return;
    }
    void* o = aEntry->mObject;
    nsCycleCollectionParticipant* cp = aEntry->mParticipant;
    CanonicalizeParticipant(&o, &cp);
    if (aEntry->mRefCnt->IsPurple() && !cp->CanSkip(o, false) &&
        (!mRemoveChildlessNodes || MayHaveChild(o, cp))) {
      return;
    }
    aBuffer.Remove(aEntry);
  }

private:
  bool mRemoveChildlessNodes;
  bool mAsyncSnowWhiteFreeing;
  bool mDispatchedDeferredDeletion;
  CC_ForgetSkippableCallback mCallback;
};

void
nsPurpleBuffer::RemoveSkippable(nsCycleCollector* aCollector,
                                bool aRemoveChildlessNodes,
                                bool aAsyncSnowWhiteFreeing,
                                CC_ForgetSkippableCallback aCb)
{
  RemoveSkippableVisitor visitor(aCollector, Count(), aRemoveChildlessNodes,
                                 aAsyncSnowWhiteFreeing, aCb);
  VisitEntries(visitor);
}

bool
nsCycleCollector::FreeSnowWhite(bool aUntilNoSWInPurpleBuffer)
{
  CheckThreadSafety();

  if (mFreeingSnowWhite) {
    return false;
  }

  AutoRestore<bool> ar(mFreeingSnowWhite);
  mFreeingSnowWhite = true;

  bool hadSnowWhiteObjects = false;
  do {
    SnowWhiteKiller visitor(this, mPurpleBuf.Count());
    mPurpleBuf.VisitEntries(visitor);
    hadSnowWhiteObjects = hadSnowWhiteObjects ||
                          visitor.HasSnowWhiteObjects();
    if (!visitor.HasSnowWhiteObjects()) {
      break;
    }
  } while (aUntilNoSWInPurpleBuffer);
  return hadSnowWhiteObjects;
}

void
nsCycleCollector::ForgetSkippable(bool aRemoveChildlessNodes,
                                  bool aAsyncSnowWhiteFreeing)
{
  CheckThreadSafety();

  mozilla::Maybe<mozilla::AutoGlobalTimelineMarker> marker;
  if (NS_IsMainThread()) {
    marker.emplace("nsCycleCollector::ForgetSkippable", MarkerStackRequest::NO_STACK);
  }

  // If we remove things from the purple buffer during graph building, we may
  // lose track of an object that was mutated during graph building.
  MOZ_ASSERT(IsIdle());

  if (mJSRuntime) {
    mJSRuntime->PrepareForForgetSkippable();
  }
  MOZ_ASSERT(!mScanInProgress,
             "Don't forget skippable or free snow-white while scan is in progress.");
  mPurpleBuf.RemoveSkippable(this, aRemoveChildlessNodes,
                             aAsyncSnowWhiteFreeing, mForgetSkippableCB);
}

MOZ_NEVER_INLINE void
nsCycleCollector::MarkRoots(SliceBudget& aBudget)
{
  TimeLog timeLog;
  AutoRestore<bool> ar(mScanInProgress);
  MOZ_ASSERT(!mScanInProgress);
  mScanInProgress = true;
  MOZ_ASSERT(mIncrementalPhase == GraphBuildingPhase);

  bool doneBuilding = mBuilder->BuildGraph(aBudget);

  if (!doneBuilding) {
    timeLog.Checkpoint("MarkRoots()");
    return;
  }

  mBuilder = nullptr;
  mIncrementalPhase = ScanAndCollectWhitePhase;
  timeLog.Checkpoint("MarkRoots()");
}


////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |ScanRoots| routine.
////////////////////////////////////////////////////////////////////////


struct ScanBlackVisitor
{
  ScanBlackVisitor(uint32_t& aWhiteNodeCount, bool& aFailed)
    : mWhiteNodeCount(aWhiteNodeCount), mFailed(aFailed)
  {
  }

  bool ShouldVisitNode(PtrInfo const* aPi)
  {
    return aPi->mColor != black;
  }

  MOZ_NEVER_INLINE void VisitNode(PtrInfo* aPi)
  {
    if (aPi->mColor == white) {
      --mWhiteNodeCount;
    }
    aPi->mColor = black;
  }

  void Failed()
  {
    mFailed = true;
  }

private:
  uint32_t& mWhiteNodeCount;
  bool& mFailed;
};

static void
FloodBlackNode(uint32_t& aWhiteNodeCount, bool& aFailed, PtrInfo* aPi)
{
  GraphWalker<ScanBlackVisitor>(ScanBlackVisitor(aWhiteNodeCount,
                                                 aFailed)).Walk(aPi);
  MOZ_ASSERT(aPi->mColor == black || !aPi->WasTraversed(),
             "FloodBlackNode should make aPi black");
}

// Iterate over the WeakMaps.  If we mark anything while iterating
// over the WeakMaps, we must iterate over all of the WeakMaps again.
void
nsCycleCollector::ScanWeakMaps()
{
  bool anyChanged;
  bool failed = false;
  do {
    anyChanged = false;
    for (uint32_t i = 0; i < mGraph.mWeakMaps.Length(); i++) {
      WeakMapping* wm = &mGraph.mWeakMaps[i];

      // If any of these are null, the original object was marked black.
      uint32_t mColor = wm->mMap ? wm->mMap->mColor : black;
      uint32_t kColor = wm->mKey ? wm->mKey->mColor : black;
      uint32_t kdColor = wm->mKeyDelegate ? wm->mKeyDelegate->mColor : black;
      uint32_t vColor = wm->mVal ? wm->mVal->mColor : black;

      MOZ_ASSERT(mColor != grey, "Uncolored weak map");
      MOZ_ASSERT(kColor != grey, "Uncolored weak map key");
      MOZ_ASSERT(kdColor != grey, "Uncolored weak map key delegate");
      MOZ_ASSERT(vColor != grey, "Uncolored weak map value");

      if (mColor == black && kColor != black && kdColor == black) {
        FloodBlackNode(mWhiteNodeCount, failed, wm->mKey);
        anyChanged = true;
      }

      if (mColor == black && kColor == black && vColor != black) {
        FloodBlackNode(mWhiteNodeCount, failed, wm->mVal);
        anyChanged = true;
      }
    }
  } while (anyChanged);

  if (failed) {
    MOZ_ASSERT(false, "Ran out of memory in ScanWeakMaps");
    CC_TELEMETRY(_OOM, true);
  }
}

// Flood black from any objects in the purple buffer that are in the CC graph.
class PurpleScanBlackVisitor
{
public:
  PurpleScanBlackVisitor(CCGraph& aGraph, nsCycleCollectorLogger* aLogger,
                         uint32_t& aCount, bool& aFailed)
    : mGraph(aGraph), mLogger(aLogger), mCount(aCount), mFailed(aFailed)
  {
  }

  void
  Visit(nsPurpleBuffer& aBuffer, nsPurpleBufferEntry* aEntry)
  {
    MOZ_ASSERT(aEntry->mObject,
               "Entries with null mObject shouldn't be in the purple buffer.");
    MOZ_ASSERT(aEntry->mRefCnt->get() != 0,
               "Snow-white objects shouldn't be in the purple buffer.");

    void* obj = aEntry->mObject;
    if (!aEntry->mParticipant) {
      obj = CanonicalizeXPCOMParticipant(static_cast<nsISupports*>(obj));
      MOZ_ASSERT(obj, "Don't add objects that don't participate in collection!");
    }

    PtrInfo* pi = mGraph.FindNode(obj);
    if (!pi) {
      return;
    }
    MOZ_ASSERT(pi->mParticipant, "No dead objects should be in the purple buffer.");
    if (MOZ_UNLIKELY(mLogger)) {
      mLogger->NoteIncrementalRoot((uint64_t)pi->mPointer);
    }
    if (pi->mColor == black) {
      return;
    }
    FloodBlackNode(mCount, mFailed, pi);
  }

private:
  CCGraph& mGraph;
  RefPtr<nsCycleCollectorLogger> mLogger;
  uint32_t& mCount;
  bool& mFailed;
};

// Objects that have been stored somewhere since the start of incremental graph building must
// be treated as live for this cycle collection, because we may not have accurate information
// about who holds references to them.
void
nsCycleCollector::ScanIncrementalRoots()
{
  TimeLog timeLog;

  // Reference counted objects:
  // We cleared the purple buffer at the start of the current ICC, so if a
  // refcounted object is purple, it may have been AddRef'd during the current
  // ICC. (It may also have only been released.) If that is the case, we cannot
  // be sure that the set of things pointing to the object in the CC graph
  // is accurate. Therefore, for safety, we treat any purple objects as being
  // live during the current CC. We don't remove anything from the purple
  // buffer here, so these objects will be suspected and freed in the next CC
  // if they are garbage.
  bool failed = false;
  PurpleScanBlackVisitor purpleScanBlackVisitor(mGraph, mLogger,
                                                mWhiteNodeCount, failed);
  mPurpleBuf.VisitEntries(purpleScanBlackVisitor);
  timeLog.Checkpoint("ScanIncrementalRoots::fix purple");

  bool hasJSRuntime = !!mJSRuntime;
  nsCycleCollectionParticipant* jsParticipant =
    hasJSRuntime ? mJSRuntime->GCThingParticipant() : nullptr;
  nsCycleCollectionParticipant* zoneParticipant =
    hasJSRuntime ? mJSRuntime->ZoneParticipant() : nullptr;
  bool hasLogger = !!mLogger;

  NodePool::Enumerator etor(mGraph.mNodes);
  while (!etor.IsDone()) {
    PtrInfo* pi = etor.GetNext();

    // As an optimization, if an object has already been determined to be live,
    // don't consider it further.  We can't do this if there is a listener,
    // because the listener wants to know the complete set of incremental roots.
    if (pi->mColor == black && MOZ_LIKELY(!hasLogger)) {
      continue;
    }

    // Garbage collected objects:
    // If a GCed object was added to the graph with a refcount of zero, and is
    // now marked black by the GC, it was probably gray before and was exposed
    // to active JS, so it may have been stored somewhere, so it needs to be
    // treated as live.
    if (pi->IsGrayJS() && MOZ_LIKELY(hasJSRuntime)) {
      // If the object is still marked gray by the GC, nothing could have gotten
      // hold of it, so it isn't an incremental root.
      if (pi->mParticipant == jsParticipant) {
        JS::GCCellPtr ptr(pi->mPointer, js::GCThingTraceKind(pi->mPointer));
        if (GCThingIsGrayCCThing(ptr)) {
          continue;
        }
      } else if (pi->mParticipant == zoneParticipant) {
        JS::Zone* zone = static_cast<JS::Zone*>(pi->mPointer);
        if (js::ZoneGlobalsAreAllGray(zone)) {
          continue;
        }
      } else {
        MOZ_ASSERT(false, "Non-JS thing with 0 refcount? Treating as live.");
      }
    } else if (!pi->mParticipant && pi->WasTraversed()) {
      // Dead traversed refcounted objects:
      // If the object was traversed, it must have been alive at the start of
      // the CC, and thus had a positive refcount. It is dead now, so its
      // refcount must have decreased at some point during the CC. Therefore,
      // it would be in the purple buffer if it wasn't dead, so treat it as an
      // incremental root.
      //
      // This should not cause leaks because as the object died it should have
      // released anything it held onto, which will add them to the purple
      // buffer, which will cause them to be considered in the next CC.
    } else {
      continue;
    }

    // At this point, pi must be an incremental root.

    // If there's a listener, tell it about this root. We don't bother with the
    // optimization of skipping the Walk() if pi is black: it will just return
    // without doing anything and there's no need to make this case faster.
    if (MOZ_UNLIKELY(hasLogger) && pi->mPointer) {
      // Dead objects aren't logged. See bug 1031370.
      mLogger->NoteIncrementalRoot((uint64_t)pi->mPointer);
    }

    FloodBlackNode(mWhiteNodeCount, failed, pi);
  }

  timeLog.Checkpoint("ScanIncrementalRoots::fix nodes");

  if (failed) {
    NS_ASSERTION(false, "Ran out of memory in ScanIncrementalRoots");
    CC_TELEMETRY(_OOM, true);
  }
}

// Mark nodes white and make sure their refcounts are ok.
// No nodes are marked black during this pass to ensure that refcount
// checking is run on all nodes not marked black by ScanIncrementalRoots.
void
nsCycleCollector::ScanWhiteNodes(bool aFullySynchGraphBuild)
{
  NodePool::Enumerator nodeEnum(mGraph.mNodes);
  while (!nodeEnum.IsDone()) {
    PtrInfo* pi = nodeEnum.GetNext();
    if (pi->mColor == black) {
      // Incremental roots can be in a nonsensical state, so don't
      // check them. This will miss checking nodes that are merely
      // reachable from incremental roots.
      MOZ_ASSERT(!aFullySynchGraphBuild,
                 "In a synch CC, no nodes should be marked black early on.");
      continue;
    }
    MOZ_ASSERT(pi->mColor == grey);

    if (!pi->WasTraversed()) {
      // This node was deleted before it was traversed, so there's no reason
      // to look at it.
      MOZ_ASSERT(!pi->mParticipant, "Live nodes should all have been traversed");
      continue;
    }

    if (pi->mInternalRefs == pi->mRefCount || pi->IsGrayJS()) {
      pi->mColor = white;
      ++mWhiteNodeCount;
      continue;
    }

    MOZ_RELEASE_ASSERT(pi->mInternalRefs < pi->mRefCount,
                       "Cycle collector found more references to an object than its refcount");

    // This node will get marked black in the next pass.
  }
}

// Any remaining grey nodes that haven't already been deleted must be alive,
// so mark them and their children black. Any nodes that are black must have
// already had their children marked black, so there's no need to look at them
// again. This pass may turn some white nodes to black.
void
nsCycleCollector::ScanBlackNodes()
{
  bool failed = false;
  NodePool::Enumerator nodeEnum(mGraph.mNodes);
  while (!nodeEnum.IsDone()) {
    PtrInfo* pi = nodeEnum.GetNext();
    if (pi->mColor == grey && pi->WasTraversed()) {
      FloodBlackNode(mWhiteNodeCount, failed, pi);
    }
  }

  if (failed) {
    NS_ASSERTION(false, "Ran out of memory in ScanBlackNodes");
    CC_TELEMETRY(_OOM, true);
  }
}

void
nsCycleCollector::ScanRoots(bool aFullySynchGraphBuild)
{
  AutoRestore<bool> ar(mScanInProgress);
  MOZ_ASSERT(!mScanInProgress);
  mScanInProgress = true;
  mWhiteNodeCount = 0;
  MOZ_ASSERT(mIncrementalPhase == ScanAndCollectWhitePhase);

  if (!aFullySynchGraphBuild) {
    ScanIncrementalRoots();
  }

  TimeLog timeLog;
  ScanWhiteNodes(aFullySynchGraphBuild);
  timeLog.Checkpoint("ScanRoots::ScanWhiteNodes");

  ScanBlackNodes();
  timeLog.Checkpoint("ScanRoots::ScanBlackNodes");

  // Scanning weak maps must be done last.
  ScanWeakMaps();
  timeLog.Checkpoint("ScanRoots::ScanWeakMaps");

  if (mLogger) {
    mLogger->BeginResults();

    NodePool::Enumerator etor(mGraph.mNodes);
    while (!etor.IsDone()) {
      PtrInfo* pi = etor.GetNext();
      if (!pi->WasTraversed()) {
        continue;
      }
      switch (pi->mColor) {
        case black:
          if (!pi->IsGrayJS() && !pi->IsBlackJS() &&
              pi->mInternalRefs != pi->mRefCount) {
            mLogger->DescribeRoot((uint64_t)pi->mPointer,
                                  pi->mInternalRefs);
          }
          break;
        case white:
          mLogger->DescribeGarbage((uint64_t)pi->mPointer);
          break;
        case grey:
          MOZ_ASSERT(false, "All traversed objects should be black or white");
          break;
      }
    }

    mLogger->End();
    mLogger = nullptr;
    timeLog.Checkpoint("ScanRoots::listener");
  }
}


////////////////////////////////////////////////////////////////////////
// Bacon & Rajan's |CollectWhite| routine, somewhat modified.
////////////////////////////////////////////////////////////////////////

bool
nsCycleCollector::CollectWhite()
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

  // Segments are 4 KiB on 32-bit and 8 KiB on 64-bit.
  static const size_t kSegmentSize = sizeof(void*) * 1024;
  SegmentedVector<PtrInfo*, kSegmentSize, InfallibleAllocPolicy>
    whiteNodes(kSegmentSize);
  TimeLog timeLog;

  MOZ_ASSERT(mIncrementalPhase == ScanAndCollectWhitePhase);

  uint32_t numWhiteNodes = 0;
  uint32_t numWhiteGCed = 0;
  uint32_t numWhiteJSZones = 0;

  bool hasJSRuntime = !!mJSRuntime;
  nsCycleCollectionParticipant* zoneParticipant =
    hasJSRuntime ? mJSRuntime->ZoneParticipant() : nullptr;

  NodePool::Enumerator etor(mGraph.mNodes);
  while (!etor.IsDone()) {
    PtrInfo* pinfo = etor.GetNext();
    if (pinfo->mColor == white && pinfo->mParticipant) {
      if (pinfo->IsGrayJS()) {
        ++numWhiteGCed;
        if (MOZ_UNLIKELY(pinfo->mParticipant == zoneParticipant)) {
          ++numWhiteJSZones;
        }
      } else {
        whiteNodes.InfallibleAppend(pinfo);
        pinfo->mParticipant->Root(pinfo->mPointer);
        ++numWhiteNodes;
      }
    }
  }

  mResults.mFreedRefCounted += numWhiteNodes;
  mResults.mFreedGCed += numWhiteGCed;
  mResults.mFreedJSZones += numWhiteJSZones;

  timeLog.Checkpoint("CollectWhite::Root");

  if (mBeforeUnlinkCB) {
    mBeforeUnlinkCB();
    timeLog.Checkpoint("CollectWhite::BeforeUnlinkCB");
  }

  // Unlink() can trigger a GC, so do not touch any JS or anything
  // else not in whiteNodes after here.

  for (auto iter = whiteNodes.Iter(); !iter.Done(); iter.Next()) {
    PtrInfo* pinfo = iter.Get();
    MOZ_ASSERT(pinfo->mParticipant,
               "Unlink shouldn't see objects removed from graph.");
    pinfo->mParticipant->Unlink(pinfo->mPointer);
#ifdef DEBUG
    if (mJSRuntime) {
      mJSRuntime->AssertNoObjectsToTrace(pinfo->mPointer);
    }
#endif
  }
  timeLog.Checkpoint("CollectWhite::Unlink");

  for (auto iter = whiteNodes.Iter(); !iter.Done(); iter.Next()) {
    PtrInfo* pinfo = iter.Get();
    MOZ_ASSERT(pinfo->mParticipant,
               "Unroot shouldn't see objects removed from graph.");
    pinfo->mParticipant->Unroot(pinfo->mPointer);
  }
  timeLog.Checkpoint("CollectWhite::Unroot");

  nsCycleCollector_dispatchDeferredDeletion(false, true);
  timeLog.Checkpoint("CollectWhite::dispatchDeferredDeletion");

  mIncrementalPhase = CleanupPhase;

  return numWhiteNodes > 0 || numWhiteGCed > 0 || numWhiteJSZones > 0;
}


////////////////////////
// Memory reporting
////////////////////////

MOZ_DEFINE_MALLOC_SIZE_OF(CycleCollectorMallocSizeOf)

NS_IMETHODIMP
nsCycleCollector::CollectReports(nsIHandleReportCallback* aHandleReport,
                                 nsISupports* aData, bool aAnonymize)
{
  size_t objectSize, graphSize, purpleBufferSize;
  SizeOfIncludingThis(CycleCollectorMallocSizeOf,
                      &objectSize, &graphSize,
                      &purpleBufferSize);

#define REPORT(_path, _amount, _desc)                                     \
    do {                                                                  \
        size_t amount = _amount;  /* evaluate |_amount| only once */      \
        if (amount > 0) {                                                 \
            nsresult rv;                                                  \
            rv = aHandleReport->Callback(EmptyCString(),                  \
                                         NS_LITERAL_CSTRING(_path),       \
                                         KIND_HEAP, UNITS_BYTES, _amount, \
                                         NS_LITERAL_CSTRING(_desc),       \
                                         aData);                          \
            if (NS_WARN_IF(NS_FAILED(rv)))                                \
                return rv;                                                \
        }                                                                 \
    } while (0)

  REPORT("explicit/cycle-collector/collector-object", objectSize,
         "Memory used for the cycle collector object itself.");

  REPORT("explicit/cycle-collector/graph", graphSize,
         "Memory used for the cycle collector's graph. "
         "This should be zero when the collector is idle.");

  REPORT("explicit/cycle-collector/purple-buffer", purpleBufferSize,
         "Memory used for the cycle collector's purple buffer.");

#undef REPORT

  return NS_OK;
};


////////////////////////////////////////////////////////////////////////
// Collector implementation
////////////////////////////////////////////////////////////////////////

nsCycleCollector::nsCycleCollector() :
  mActivelyCollecting(false),
  mFreeingSnowWhite(false),
  mScanInProgress(false),
  mJSRuntime(nullptr),
  mIncrementalPhase(IdlePhase),
  mThread(NS_GetCurrentThread()),
  mWhiteNodeCount(0),
  mBeforeUnlinkCB(nullptr),
  mForgetSkippableCB(nullptr),
  mUnmergedNeeded(0),
  mMergedInARow(0)
{
}

nsCycleCollector::~nsCycleCollector()
{
  UnregisterWeakMemoryReporter(this);
}

void
nsCycleCollector::RegisterJSRuntime(CycleCollectedJSRuntime* aJSRuntime)
{
  MOZ_RELEASE_ASSERT(!mJSRuntime, "Multiple registrations of JS runtime in cycle collector");
  mJSRuntime = aJSRuntime;

  // We can't register as a reporter in nsCycleCollector() because that runs
  // before the memory reporter manager is initialized.  So we do it here
  // instead.
  static bool registered = false;
  if (!registered) {
    RegisterWeakMemoryReporter(this);
    registered = true;
  }
}

void
nsCycleCollector::ForgetJSRuntime()
{
  MOZ_RELEASE_ASSERT(mJSRuntime, "Forgetting JS runtime in cycle collector before a JS runtime was registered");
  mJSRuntime = nullptr;
}

#ifdef DEBUG
static bool
HasParticipant(void* aPtr, nsCycleCollectionParticipant* aParti)
{
  if (aParti) {
    return true;
  }

  nsXPCOMCycleCollectionParticipant* xcp;
  ToParticipant(static_cast<nsISupports*>(aPtr), &xcp);
  return xcp != nullptr;
}
#endif

MOZ_ALWAYS_INLINE void
nsCycleCollector::Suspect(void* aPtr, nsCycleCollectionParticipant* aParti,
                          nsCycleCollectingAutoRefCnt* aRefCnt)
{
  CheckThreadSafety();

  // Don't call AddRef or Release of a CCed object in a Traverse() method.
  MOZ_ASSERT(!mScanInProgress, "Attempted to call Suspect() while a scan was in progress");

  if (MOZ_UNLIKELY(mScanInProgress)) {
    return;
  }

  MOZ_ASSERT(aPtr, "Don't suspect null pointers");

  MOZ_ASSERT(HasParticipant(aPtr, aParti),
             "Suspected nsISupports pointer must QI to nsXPCOMCycleCollectionParticipant");

  mPurpleBuf.Put(aPtr, aParti, aRefCnt);
}

void
nsCycleCollector::CheckThreadSafety()
{
#ifdef DEBUG
  nsIThread* currentThread = NS_GetCurrentThread();
  // XXXkhuey we can be called so late in shutdown that NS_GetCurrentThread
  // returns null (after the thread manager has shut down)
  MOZ_ASSERT(mThread == currentThread || !currentThread);
#endif
}

// The cycle collector uses the mark bitmap to discover what JS objects
// were reachable only from XPConnect roots that might participate in
// cycles. We ask the JS runtime whether we need to force a GC before
// this CC. It returns true on startup (before the mark bits have been set),
// and also when UnmarkGray has run out of stack.  We also force GCs on shut
// down to collect cycles involving both DOM and JS.
void
nsCycleCollector::FixGrayBits(bool aForceGC, TimeLog& aTimeLog)
{
  CheckThreadSafety();

  if (!mJSRuntime) {
    return;
  }

  if (!aForceGC) {
    mJSRuntime->FixWeakMappingGrayBits();
    aTimeLog.Checkpoint("FixWeakMappingGrayBits");

    bool needGC = !mJSRuntime->AreGCGrayBitsValid();
    // Only do a telemetry ping for non-shutdown CCs.
    CC_TELEMETRY(_NEED_GC, needGC);
    if (!needGC) {
      return;
    }
    mResults.mForcedGC = true;
  }

  mJSRuntime->GarbageCollect(aForceGC ? JS::gcreason::SHUTDOWN_CC :
                                        JS::gcreason::CC_FORCED);
  aTimeLog.Checkpoint("FixGrayBits GC");
}

bool
nsCycleCollector::IsIncrementalGCInProgress()
{
  return mJSRuntime && JS::IsIncrementalGCInProgress(mJSRuntime->Runtime());
}

void
nsCycleCollector::FinishAnyIncrementalGCInProgress()
{
  if (IsIncrementalGCInProgress()) {
    NS_WARNING("Finishing incremental GC in progress during CC");
    JS::PrepareForIncrementalGC(mJSRuntime->Runtime());
    JS::FinishIncrementalGC(mJSRuntime->Runtime(), JS::gcreason::CC_FORCED);
  }
}

void
nsCycleCollector::CleanupAfterCollection()
{
  TimeLog timeLog;
  MOZ_ASSERT(mIncrementalPhase == CleanupPhase);
  mGraph.Clear();
  timeLog.Checkpoint("CleanupAfterCollection::mGraph.Clear()");

  uint32_t interval =
    (uint32_t)((TimeStamp::Now() - mCollectionStart).ToMilliseconds());
#ifdef COLLECT_TIME_DEBUG
  printf("cc: total cycle collector time was %ums in %u slices\n", interval,
         mResults.mNumSlices);
  printf("cc: visited %u ref counted and %u GCed objects, freed %d ref counted and %d GCed objects",
         mResults.mVisitedRefCounted, mResults.mVisitedGCed,
         mResults.mFreedRefCounted, mResults.mFreedGCed);
  uint32_t numVisited = mResults.mVisitedRefCounted + mResults.mVisitedGCed;
  if (numVisited > 1000) {
    uint32_t numFreed = mResults.mFreedRefCounted + mResults.mFreedGCed;
    printf(" (%d%%)", 100 * numFreed / numVisited);
  }
  printf(".\ncc: \n");
#endif

  CC_TELEMETRY( , interval);
  CC_TELEMETRY(_VISITED_REF_COUNTED, mResults.mVisitedRefCounted);
  CC_TELEMETRY(_VISITED_GCED, mResults.mVisitedGCed);
  CC_TELEMETRY(_COLLECTED, mWhiteNodeCount);
  timeLog.Checkpoint("CleanupAfterCollection::telemetry");

  if (mJSRuntime) {
    mJSRuntime->FinalizeDeferredThings(mResults.mAnyManual
                                       ? CycleCollectedJSRuntime::FinalizeNow
                                       : CycleCollectedJSRuntime::FinalizeIncrementally);
    mJSRuntime->EndCycleCollectionCallback(mResults);
    timeLog.Checkpoint("CleanupAfterCollection::EndCycleCollectionCallback()");
  }
  mIncrementalPhase = IdlePhase;
}

void
nsCycleCollector::ShutdownCollect()
{
  FinishAnyIncrementalGCInProgress();

  SliceBudget unlimitedBudget = SliceBudget::unlimited();
  uint32_t i;
  for (i = 0; i < DEFAULT_SHUTDOWN_COLLECTIONS; ++i) {
    if (!Collect(ShutdownCC, unlimitedBudget, nullptr)) {
      break;
    }
  }
  NS_WARN_IF_FALSE(i < NORMAL_SHUTDOWN_COLLECTIONS, "Extra shutdown CC");
}

static void
PrintPhase(const char* aPhase)
{
#ifdef DEBUG_PHASES
  printf("cc: begin %s on %s\n", aPhase,
         NS_IsMainThread() ? "mainthread" : "worker");
#endif
}

bool
nsCycleCollector::Collect(ccType aCCType,
                          SliceBudget& aBudget,
                          nsICycleCollectorListener* aManualListener,
                          bool aPreferShorterSlices)
{
  CheckThreadSafety();

  // This can legitimately happen in a few cases. See bug 383651.
  if (mActivelyCollecting || mFreeingSnowWhite) {
    return false;
  }
  mActivelyCollecting = true;

  MOZ_ASSERT(!IsIncrementalGCInProgress());

  mozilla::Maybe<mozilla::AutoGlobalTimelineMarker> marker;
  if (NS_IsMainThread()) {
    marker.emplace("nsCycleCollector::Collect", MarkerStackRequest::NO_STACK);
  }

  bool startedIdle = IsIdle();
  bool collectedAny = false;

  // If the CC started idle, it will call BeginCollection, which
  // will do FreeSnowWhite, so it doesn't need to be done here.
  if (!startedIdle) {
    TimeLog timeLog;
    FreeSnowWhite(true);
    timeLog.Checkpoint("Collect::FreeSnowWhite");
  }

  if (aCCType != SliceCC) {
    mResults.mAnyManual = true;
  }

  ++mResults.mNumSlices;

  bool continueSlice = aBudget.isUnlimited() || !aPreferShorterSlices;
  do {
    switch (mIncrementalPhase) {
      case IdlePhase:
        PrintPhase("BeginCollection");
        BeginCollection(aCCType, aManualListener);
        break;
      case GraphBuildingPhase:
        PrintPhase("MarkRoots");
        MarkRoots(aBudget);

        // Only continue this slice if we're running synchronously or the
        // next phase will probably be short, to reduce the max pause for this
        // collection.
        // (There's no need to check if we've finished graph building, because
        // if we haven't, we've already exceeded our budget, and will finish
        // this slice anyways.)
        continueSlice = aBudget.isUnlimited() ||
          (mResults.mNumSlices < 3 && !aPreferShorterSlices);
        break;
      case ScanAndCollectWhitePhase:
        // We do ScanRoots and CollectWhite in a single slice to ensure
        // that we won't unlink a live object if a weak reference is
        // promoted to a strong reference after ScanRoots has finished.
        // See bug 926533.
        PrintPhase("ScanRoots");
        ScanRoots(startedIdle);
        PrintPhase("CollectWhite");
        collectedAny = CollectWhite();
        break;
      case CleanupPhase:
        PrintPhase("CleanupAfterCollection");
        CleanupAfterCollection();
        continueSlice = false;
        break;
    }
    if (continueSlice) {
      // Force SliceBudget::isOverBudget to check the time.
      aBudget.step(SliceBudget::CounterReset);
      continueSlice = !aBudget.isOverBudget();
    }
  } while (continueSlice);

  // Clear mActivelyCollecting here to ensure that a recursive call to
  // Collect() does something.
  mActivelyCollecting = false;

  if (aCCType != SliceCC && !startedIdle) {
    // We were in the middle of an incremental CC (using its own listener).
    // Somebody has forced a CC, so after having finished out the current CC,
    // run the CC again using the new listener.
    MOZ_ASSERT(IsIdle());
    if (Collect(aCCType, aBudget, aManualListener)) {
      collectedAny = true;
    }
  }

  MOZ_ASSERT_IF(aCCType != SliceCC, IsIdle());

  return collectedAny;
}

// Any JS objects we have in the graph could die when we GC, but we
// don't want to abandon the current CC, because the graph contains
// information about purple roots. So we synchronously finish off
// the current CC.
void
nsCycleCollector::PrepareForGarbageCollection()
{
  if (IsIdle()) {
    MOZ_ASSERT(mGraph.IsEmpty(), "Non-empty graph when idle");
    MOZ_ASSERT(!mBuilder, "Non-null builder when idle");
    if (mJSPurpleBuffer) {
      mJSPurpleBuffer->Destroy();
    }
    return;
  }

  FinishAnyCurrentCollection();
}

void
nsCycleCollector::FinishAnyCurrentCollection()
{
  if (IsIdle()) {
    return;
  }

  SliceBudget unlimitedBudget = SliceBudget::unlimited();
  PrintPhase("FinishAnyCurrentCollection");
  // Use SliceCC because we only want to finish the CC in progress.
  Collect(SliceCC, unlimitedBudget, nullptr);

  // It is only okay for Collect() to have failed to finish the
  // current CC if we're reentering the CC at some point past
  // graph building. We need to be past the point where the CC will
  // look at JS objects so that it is safe to GC.
  MOZ_ASSERT(IsIdle() ||
             (mActivelyCollecting && mIncrementalPhase != GraphBuildingPhase),
             "Reentered CC during graph building");
}

// Don't merge too many times in a row, and do at least a minimum
// number of unmerged CCs in a row.
static const uint32_t kMinConsecutiveUnmerged = 3;
static const uint32_t kMaxConsecutiveMerged = 3;

bool
nsCycleCollector::ShouldMergeZones(ccType aCCType)
{
  if (!mJSRuntime) {
    return false;
  }

  MOZ_ASSERT(mUnmergedNeeded <= kMinConsecutiveUnmerged);
  MOZ_ASSERT(mMergedInARow <= kMaxConsecutiveMerged);

  if (mMergedInARow == kMaxConsecutiveMerged) {
    MOZ_ASSERT(mUnmergedNeeded == 0);
    mUnmergedNeeded = kMinConsecutiveUnmerged;
  }

  if (mUnmergedNeeded > 0) {
    mUnmergedNeeded--;
    mMergedInARow = 0;
    return false;
  }

  if (aCCType == SliceCC && mJSRuntime->UsefulToMergeZones()) {
    mMergedInARow++;
    return true;
  } else {
    mMergedInARow = 0;
    return false;
  }
}

void
nsCycleCollector::BeginCollection(ccType aCCType,
                                  nsICycleCollectorListener* aManualListener)
{
  TimeLog timeLog;
  MOZ_ASSERT(IsIdle());

  mCollectionStart = TimeStamp::Now();

  if (mJSRuntime) {
    mJSRuntime->BeginCycleCollectionCallback();
    timeLog.Checkpoint("BeginCycleCollectionCallback()");
  }

  bool isShutdown = (aCCType == ShutdownCC);

  // Set up the listener for this CC.
  MOZ_ASSERT_IF(isShutdown, !aManualListener);
  MOZ_ASSERT(!mLogger, "Forgot to clear a previous listener?");

  if (aManualListener) {
    aManualListener->AsLogger(getter_AddRefs(mLogger));
  }

  aManualListener = nullptr;
  if (!mLogger && mParams.LogThisCC(isShutdown)) {
    mLogger = new nsCycleCollectorLogger();
    if (mParams.AllTracesThisCC(isShutdown)) {
      mLogger->SetAllTraces();
    }
  }

  // On a WantAllTraces CC, force a synchronous global GC to prevent
  // hijinks from ForgetSkippable and compartmental GCs.
  bool forceGC = isShutdown || (mLogger && mLogger->IsAllTraces());

  // BeginCycleCollectionCallback() might have started an IGC, and we need
  // to finish it before we run FixGrayBits.
  FinishAnyIncrementalGCInProgress();
  timeLog.Checkpoint("Pre-FixGrayBits finish IGC");

  FixGrayBits(forceGC, timeLog);

  FreeSnowWhite(true);
  timeLog.Checkpoint("BeginCollection FreeSnowWhite");

  if (mLogger && NS_FAILED(mLogger->Begin())) {
    mLogger = nullptr;
  }

  // FreeSnowWhite could potentially have started an IGC, which we need
  // to finish before we look at any JS roots.
  FinishAnyIncrementalGCInProgress();
  timeLog.Checkpoint("Post-FreeSnowWhite finish IGC");

  // Set up the data structures for building the graph.
  mGraph.Init();
  mResults.Init();
  mResults.mAnyManual = (aCCType != SliceCC);
  bool mergeZones = ShouldMergeZones(aCCType);
  mResults.mMergedZones = mergeZones;

  MOZ_ASSERT(!mBuilder, "Forgot to clear mBuilder");
  mBuilder = new CCGraphBuilder(mGraph, mResults, mJSRuntime, mLogger,
                                mergeZones);
  timeLog.Checkpoint("BeginCollection prepare graph builder");

  if (mJSRuntime) {
    mJSRuntime->TraverseRoots(*mBuilder);
    timeLog.Checkpoint("mJSRuntime->TraverseRoots()");
  }

  AutoRestore<bool> ar(mScanInProgress);
  MOZ_ASSERT(!mScanInProgress);
  mScanInProgress = true;
  mPurpleBuf.SelectPointers(*mBuilder);
  timeLog.Checkpoint("SelectPointers()");

  mBuilder->DoneAddingRoots();
  mIncrementalPhase = GraphBuildingPhase;
}

uint32_t
nsCycleCollector::SuspectedCount()
{
  CheckThreadSafety();
  return mPurpleBuf.Count();
}

void
nsCycleCollector::Shutdown()
{
  CheckThreadSafety();

  // Always delete snow white objects.
  FreeSnowWhite(true);

#ifndef NS_FREE_PERMANENT_DATA
  if (PR_GetEnv("MOZ_CC_RUN_DURING_SHUTDOWN"))
#endif
  {
    ShutdownCollect();
  }
}

void
nsCycleCollector::RemoveObjectFromGraph(void* aObj)
{
  if (IsIdle()) {
    return;
  }

  mGraph.RemoveObjectFromMap(aObj);
}

void
nsCycleCollector::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                      size_t* aObjectSize,
                                      size_t* aGraphSize,
                                      size_t* aPurpleBufferSize) const
{
  *aObjectSize = aMallocSizeOf(this);

  *aGraphSize = mGraph.SizeOfExcludingThis(aMallocSizeOf);

  *aPurpleBufferSize = mPurpleBuf.SizeOfExcludingThis(aMallocSizeOf);

  // These fields are deliberately not measured:
  // - mJSRuntime: because it's non-owning and measured by JS reporters.
  // - mParams: because it only contains scalars.
}

JSPurpleBuffer*
nsCycleCollector::GetJSPurpleBuffer()
{
  if (!mJSPurpleBuffer) {
    // The Release call here confuses the GC analysis.
    JS::AutoSuppressGCAnalysis nogc;
    // JSPurpleBuffer keeps itself alive, but we need to create it in such way
    // that it ends up in the normal purple buffer. That happens when
    // nsRefPtr goes out of the scope and calls Release.
    RefPtr<JSPurpleBuffer> pb = new JSPurpleBuffer(mJSPurpleBuffer);
  }
  return mJSPurpleBuffer;
}

////////////////////////////////////////////////////////////////////////
// Module public API (exported in nsCycleCollector.h)
// Just functions that redirect into the singleton, once it's built.
////////////////////////////////////////////////////////////////////////

void
nsCycleCollector_registerJSRuntime(CycleCollectedJSRuntime* aRt)
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  MOZ_ASSERT(data->mCollector);
  // But we shouldn't already have a runtime.
  MOZ_ASSERT(!data->mRuntime);

  data->mRuntime = aRt;
  data->mCollector->RegisterJSRuntime(aRt);
}

void
nsCycleCollector_forgetJSRuntime()
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  // And we shouldn't have already forgotten our runtime.
  MOZ_ASSERT(data->mRuntime);

  // But it may have shutdown already.
  if (data->mCollector) {
    data->mCollector->ForgetJSRuntime();
    data->mRuntime = nullptr;
  } else {
    data->mRuntime = nullptr;
    delete data;
    sCollectorData.set(nullptr);
  }
}

/* static */ CycleCollectedJSRuntime*
CycleCollectedJSRuntime::Get()
{
  CollectorData* data = sCollectorData.get();
  if (data) {
    return data->mRuntime;
  }
  return nullptr;
}

MOZ_NEVER_INLINE static void
SuspectAfterShutdown(void* aPtr, nsCycleCollectionParticipant* aCp,
                     nsCycleCollectingAutoRefCnt* aRefCnt,
                     bool* aShouldDelete)
{
  if (aRefCnt->get() == 0) {
    if (!aShouldDelete) {
      // The CC is shut down, so we can't be in the middle of an ICC.
      CanonicalizeParticipant(&aPtr, &aCp);
      aRefCnt->stabilizeForDeletion();
      aCp->DeleteCycleCollectable(aPtr);
    } else {
      *aShouldDelete = true;
    }
  } else {
    // Make sure we'll get called again.
    aRefCnt->RemoveFromPurpleBuffer();
  }
}

void
NS_CycleCollectorSuspect3(void* aPtr, nsCycleCollectionParticipant* aCp,
                          nsCycleCollectingAutoRefCnt* aRefCnt,
                          bool* aShouldDelete)
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);

  if (MOZ_LIKELY(data->mCollector)) {
    data->mCollector->Suspect(aPtr, aCp, aRefCnt);
    return;
  }
  SuspectAfterShutdown(aPtr, aCp, aRefCnt, aShouldDelete);
}

uint32_t
nsCycleCollector_suspectedCount()
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);

  if (!data->mCollector) {
    return 0;
  }

  return data->mCollector->SuspectedCount();
}

bool
nsCycleCollector_init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(!sCollectorData.initialized(), "Called twice!?");

  return sCollectorData.init();
}

void
nsCycleCollector_startup()
{
  MOZ_ASSERT(sCollectorData.initialized(),
             "Forgot to call nsCycleCollector_init!");
  if (sCollectorData.get()) {
    MOZ_CRASH();
  }

  CollectorData* data = new CollectorData;
  data->mCollector = new nsCycleCollector();
  data->mRuntime = nullptr;

  sCollectorData.set(data);
}

void
nsCycleCollector_setBeforeUnlinkCallback(CC_BeforeUnlinkCallback aCB)
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  MOZ_ASSERT(data->mCollector);

  data->mCollector->SetBeforeUnlinkCallback(aCB);
}

void
nsCycleCollector_setForgetSkippableCallback(CC_ForgetSkippableCallback aCB)
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  MOZ_ASSERT(data->mCollector);

  data->mCollector->SetForgetSkippableCallback(aCB);
}

void
nsCycleCollector_forgetSkippable(bool aRemoveChildlessNodes,
                                 bool aAsyncSnowWhiteFreeing)
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  MOZ_ASSERT(data->mCollector);

  PROFILER_LABEL("nsCycleCollector", "forgetSkippable",
                 js::ProfileEntry::Category::CC);

  TimeLog timeLog;
  data->mCollector->ForgetSkippable(aRemoveChildlessNodes,
                                    aAsyncSnowWhiteFreeing);
  timeLog.Checkpoint("ForgetSkippable()");
}

void
nsCycleCollector_dispatchDeferredDeletion(bool aContinuation, bool aPurge)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  if (rt) {
    rt->DispatchDeferredDeletion(aContinuation, aPurge);
  }
}

bool
nsCycleCollector_doDeferredDeletion()
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  MOZ_ASSERT(data->mCollector);
  MOZ_ASSERT(data->mRuntime);

  return data->mCollector->FreeSnowWhite(false);
}

already_AddRefed<nsICycleCollectorLogSink>
nsCycleCollector_createLogSink()
{
  nsCOMPtr<nsICycleCollectorLogSink> sink = new nsCycleCollectorLogSinkToFile();
  return sink.forget();
}

void
nsCycleCollector_collect(nsICycleCollectorListener* aManualListener)
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  MOZ_ASSERT(data->mCollector);

  PROFILER_LABEL("nsCycleCollector", "collect",
                 js::ProfileEntry::Category::CC);

  SliceBudget unlimitedBudget = SliceBudget::unlimited();
  data->mCollector->Collect(ManualCC, unlimitedBudget, aManualListener);
}

void
nsCycleCollector_collectSlice(SliceBudget& budget,
                              bool aPreferShorterSlices)
{
  CollectorData* data = sCollectorData.get();

  // We should have started the cycle collector by now.
  MOZ_ASSERT(data);
  MOZ_ASSERT(data->mCollector);

  PROFILER_LABEL("nsCycleCollector", "collectSlice",
                 js::ProfileEntry::Category::CC);

  data->mCollector->Collect(SliceCC, budget, nullptr, aPreferShorterSlices);
}

void
nsCycleCollector_prepareForGarbageCollection()
{
  CollectorData* data = sCollectorData.get();

  MOZ_ASSERT(data);

  if (!data->mCollector) {
    return;
  }

  data->mCollector->PrepareForGarbageCollection();
}

void
nsCycleCollector_finishAnyCurrentCollection()
{
  CollectorData* data = sCollectorData.get();

  MOZ_ASSERT(data);

  if (!data->mCollector) {
    return;
  }

  data->mCollector->FinishAnyCurrentCollection();
}

void
nsCycleCollector_shutdown()
{
  CollectorData* data = sCollectorData.get();

  if (data) {
    MOZ_ASSERT(data->mCollector);
    PROFILER_LABEL("nsCycleCollector", "shutdown",
                   js::ProfileEntry::Category::CC);

    data->mCollector->Shutdown();
    data->mCollector = nullptr;
    if (!data->mRuntime) {
      delete data;
      sCollectorData.set(nullptr);
    }
  }
}
