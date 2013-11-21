/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_DeadlockDetector_h
#define mozilla_DeadlockDetector_h

#include "mozilla/Attributes.h"

#include <stdlib.h>

#include "plhash.h"
#include "prlock.h"

#include "nsTArray.h"

#ifdef NS_TRACE_MALLOC
#  include "nsTraceMalloc.h"
#endif  // ifdef NS_TRACE_MALLOC

namespace mozilla {


// FIXME bug 456272: split this off into a convenience API on top of
// nsStackWalk?
class NS_COM_GLUE CallStack
{
private:
#ifdef NS_TRACE_MALLOC
    typedef nsTMStackTraceID callstack_id;
    // needs to be a macro to avoid disturbing the backtrace
#   define NS_GET_BACKTRACE() NS_TraceMallocGetStackTrace()
#   define NS_DEADLOCK_DETECTOR_CONSTEXPR
#else
    typedef void* callstack_id;
#   define NS_GET_BACKTRACE() 0
#   define NS_DEADLOCK_DETECTOR_CONSTEXPR MOZ_CONSTEXPR
#endif  // ifdef NS_TRACE_MALLOC

    callstack_id mCallStack;

public:
    /**
     * CallStack
     * *ALWAYS* *ALWAYS* *ALWAYS* call this with no arguments.  This
     * constructor takes an argument *ONLY* so that |GET_BACKTRACE()|
     * can be evaluated in the stack frame of the caller, rather than
     * that of the constructor.
     *
     * *BEWARE*: this means that calling this constructor with no
     * arguments is not the same as a "default, do-nothing"
     * constructor: it *will* construct a backtrace.  This can cause
     * unexpected performance issues.
     */
    NS_DEADLOCK_DETECTOR_CONSTEXPR
    CallStack(const callstack_id aCallStack = NS_GET_BACKTRACE()) :
        mCallStack(aCallStack)
    {
    }
    NS_DEADLOCK_DETECTOR_CONSTEXPR
    CallStack(const CallStack& aFrom) :
        mCallStack(aFrom.mCallStack)
    {
    }
    CallStack& operator=(const CallStack& aFrom)
    {
        mCallStack = aFrom.mCallStack;
        return *this;
    }
    bool operator==(const CallStack& aOther) const
    {
        return mCallStack == aOther.mCallStack;
    }
    bool operator!=(const CallStack& aOther) const
    {
        return mCallStack != aOther.mCallStack;
    }

    // FIXME bug 456272: if this is split off,
    // NS_TraceMallocPrintStackTrace should be modified to print into
    // an nsACString
    void Print(FILE* f) const
    {
#ifdef NS_TRACE_MALLOC
        if (this != &kNone && mCallStack) {
            NS_TraceMallocPrintStackTrace(f, mCallStack);
            return;
        }
#endif
        fputs("  [stack trace unavailable]\n", f);
    }

    /** The "null" callstack. */
    static const CallStack kNone;
};


/**
 * DeadlockDetector
 *
 * The following is an approximate description of how the deadlock detector
 * works.
 *
 * The deadlock detector ensures that all blocking resources are
 * acquired according to a partial order P.  One type of blocking
 * resource is a lock.  If a lock l1 is acquired (locked) before l2,
 * then we say that |l1 <_P l2|.  The detector flags an error if two
 * locks l1 and l2 have an inconsistent ordering in P; that is, if
 * both |l1 <_P l2| and |l2 <_P l1|.  This is a potential error
 * because a thread acquiring l1,l2 according to the first order might
 * race with a thread acquiring them according to the second order.
 * If this happens under the right conditions, then the acquisitions
 * will deadlock.
 *
 * This deadlock detector doesn't know at compile-time what P is.  So,
 * it tries to discover the order at run time.  More precisely, it
 * finds <i>some</i> order P, then tries to find chains of resource
 * acquisitions that violate P.  An example acquisition sequence, and
 * the orders they impose, is
 *   l1.lock()   // current chain: [ l1 ]
 *               // order: { }
 *
 *   l2.lock()   // current chain: [ l1, l2 ]
 *               // order: { l1 <_P l2 }
 *
 *   l3.lock()   // current chain: [ l1, l2, l3 ]
 *               // order: { l1 <_P l2, l2 <_P l3, l1 <_P l3 }
 *               // (note: <_P is transitive, so also |l1 <_P l3|)
 *
 *   l2.unlock() // current chain: [ l1, l3 ]
 *               // order: { l1 <_P l2, l2 <_P l3, l1 <_P l3 }
 *               // (note: it's OK, but weird, that l2 was unlocked out
 *               //  of order.  we still have l1 <_P l3).
 *
 *   l2.lock()   // current chain: [ l1, l3, l2 ]
 *               // order: { l1 <_P l2, l2 <_P l3, l1 <_P l3,
 *                                      l3 <_P l2 (!!!) }
 * BEEP BEEP!  Here the detector will flag a potential error, since
 * l2 and l3 were used inconsistently (and potentially in ways that
 * would deadlock).
 */
template <typename T>
class DeadlockDetector
{
public:
    /**
     * ResourceAcquisition
     * Consists simply of a resource and the calling context from
     * which it was acquired.  We pack this information together so
     * that it can be returned back to the caller when a potential
     * deadlock has been found.
     */
    struct ResourceAcquisition
    {
        const T* mResource;
        CallStack mCallContext;

        ResourceAcquisition(
            const T* aResource,
            const CallStack aCallContext=CallStack::kNone) :
            mResource(aResource),
            mCallContext(aCallContext)
        {
        }
        ResourceAcquisition(const ResourceAcquisition& aFrom) :
            mResource(aFrom.mResource),
            mCallContext(aFrom.mCallContext)
        {
        }
        ResourceAcquisition& operator=(const ResourceAcquisition& aFrom)
        {
            mResource = aFrom.mResource;
            mCallContext = aFrom.mCallContext;
            return *this;
        }
    };
    typedef nsTArray<ResourceAcquisition> ResourceAcquisitionArray;

private:
    typedef nsTArray<PLHashEntry*> HashEntryArray;
    typedef typename HashEntryArray::index_type index_type;
    typedef typename HashEntryArray::size_type size_type;
    enum {
        NoIndex = HashEntryArray::NoIndex
    };

    /**
     * Value type for the ordering table.  Contains the other
     * resources on which an ordering constraint |key < other|
     * exists.  The catch is that we also store the calling context at
     * which the other resource was acquired; this improves the
     * quality of error messages when potential deadlock is detected.
     */
    struct OrderingEntry
    {
        OrderingEntry() :
            mFirstSeen(CallStack::kNone),
            mOrderedLT()        // FIXME bug 456272: set to empirical
        {                       // dep size?
        }
        ~OrderingEntry()
        {
        }

        CallStack mFirstSeen; // first site from which the resource appeared
        HashEntryArray mOrderedLT; // this <_o Other
    };

    static void* TableAlloc(void* /*pool*/, size_t size)
    {
        return operator new(size);
    }
    static void TableFree(void* /*pool*/, void* item)
    {
        operator delete(item);
    }
    static PLHashEntry* EntryAlloc(void* /*pool*/, const void* key)
    {
        return new PLHashEntry;
    }
    static void EntryFree(void* /*pool*/, PLHashEntry* entry, unsigned flag)
    {
        delete static_cast<T*>(const_cast<void*>(entry->key));
        delete static_cast<OrderingEntry*>(entry->value);
        entry->value = 0;
        if (HT_FREE_ENTRY == flag)
            delete entry;
    }
    static PLHashNumber HashKey(const void* aKey)
    {
        return NS_PTR_TO_INT32(aKey) >> 2;
    }
    static const PLHashAllocOps kAllocOps;

    // Hash table "interface" the rest of the code should use

    PLHashEntry** GetEntry(const T* aKey)
    {
        return PL_HashTableRawLookup(mOrdering, HashKey(aKey), aKey);
    }

    void PutEntry(T* aKey)
    {
        PL_HashTableAdd(mOrdering, aKey, new OrderingEntry());
    }

    // XXX need these helper methods because OrderingEntry doesn't have
    // XXX access to underlying PLHashEntry

    /**
     * Add the order |aFirst <_o aSecond|.
     *
     * WARNING: this does not check whether it's sane to add this
     * order.  In the "best" bad case, when this order already exists,
     * adding it anyway may unnecessarily result in O(n^2) space.  In
     * the "worst" bad case, adding it anyway will cause
     * |InTransitiveClosure()| to diverge.
     */
    void AddOrder(PLHashEntry* aLT, PLHashEntry* aGT)
    {
        static_cast<OrderingEntry*>(aLT->value)->mOrderedLT
            .InsertElementSorted(aGT);
    }

    /**
     * Return true iff the order |aFirst < aSecond| has been
     * *explicitly* added.
     *
     * Does not consider transitivity.
     */
    bool IsOrdered(const PLHashEntry* aFirst, const PLHashEntry* aSecond)
        const
    {
        return NoIndex !=
            static_cast<const OrderingEntry*>(aFirst->value)->mOrderedLT
                .BinaryIndexOf(aSecond);
    }

    /**
     * Return a pointer to the array of all elements "that" for
     * which the order |this < that| has been explicitly added.
     *
     * NOTE: this does *not* consider transitive orderings.
     */
    PLHashEntry* const* GetOrders(const PLHashEntry* aEntry) const
    {
        return static_cast<const OrderingEntry*>(aEntry->value)->mOrderedLT
            .Elements();
    }

    /**
     * Return the number of elements "that" for which the order
     * |this < that| has been explicitly added.
     *
     * NOTE: this does *not* consider transitive orderings.
     */
    size_type NumOrders(const PLHashEntry* aEntry) const
    {
        return static_cast<const OrderingEntry*>(aEntry->value)->mOrderedLT
            .Length();
    }

    /** Make a ResourceAcquisition out of |aEntry|. */
    ResourceAcquisition MakeResourceAcquisition(const PLHashEntry* aEntry)
        const
    {
        return ResourceAcquisition(
            static_cast<const T*>(aEntry->key),
            static_cast<const OrderingEntry*>(aEntry->value)->mFirstSeen);
    }

    // Throwaway RAII lock to make the following code safer.
    struct PRAutoLock
    {
        PRAutoLock(PRLock* aLock) : mLock(aLock) { PR_Lock(mLock); }
        ~PRAutoLock() { PR_Unlock(mLock); }
        PRLock* mLock;
    };

public:
    static const uint32_t kDefaultNumBuckets;

    /**
     * DeadlockDetector
     * Create a new deadlock detector.
     *
     * @param aNumResourcesGuess Guess at approximate number of resources
     *        that will be checked.
     */
    DeadlockDetector(uint32_t aNumResourcesGuess = kDefaultNumBuckets)
    {
        mOrdering = PL_NewHashTable(aNumResourcesGuess,
                                    HashKey,
                                    PL_CompareValues, PL_CompareValues,
                                    &kAllocOps, 0);
        if (!mOrdering)
            NS_RUNTIMEABORT("couldn't initialize resource ordering table");

        mLock = PR_NewLock();
        if (!mLock)
            NS_RUNTIMEABORT("couldn't allocate deadlock detector lock");
    }

    /**
     * ~DeadlockDetector
     *
     * *NOT* thread safe.
     */
    ~DeadlockDetector()
    {
        PL_HashTableDestroy(mOrdering);
        PR_DestroyLock(mLock);
    }

    /**
     * Add
     * Make the deadlock detector aware of |aResource|.
     *
     * WARNING: The deadlock detector owns |aResource|.
     *
     * Thread safe.
     *
     * @param aResource Resource to make deadlock detector aware of.
     */
    void Add(T* aResource)
    {
        PRAutoLock _(mLock);
        PutEntry(aResource);
    }

    // Nb: implementing a Remove() method makes the detector "more
    // unsound."  By removing a resource from the orderings, deadlocks
    // may be missed that would otherwise have been found.  However,
    // removing resources possibly reduces the # of false positives,
    // and additionally saves space.  So it's a trade off; we have
    // chosen to err on the side of caution and not implement Remove().

    /**
     * CheckAcquisition This method is called after acquiring |aLast|,
     * but before trying to acquire |aProposed| from |aCallContext|.
     * It determines whether actually trying to acquire |aProposed|
     * will create problems.  It is OK if |aLast| is nullptr; this is
     * interpreted as |aProposed| being the thread's first acquisition
     * of its current chain.
     *
     * Iff acquiring |aProposed| may lead to deadlock for some thread
     * interleaving (including the current one!), the cyclical
     * dependency from which this was deduced is returned.  Otherwise,
     * 0 is returned.
     *
     * If a potential deadlock is detected and a resource cycle is
     * returned, it is the *caller's* responsibility to free it.
     *
     * Thread safe.
     *
     * @param aLast Last resource acquired by calling thread (or 0).
     * @param aProposed Resource calling thread proposes to acquire.
     * @param aCallContext Calling context whence acquisiton request came.
     */
    ResourceAcquisitionArray* CheckAcquisition(const T* aLast,
                                               const T* aProposed,
                                               const CallStack& aCallContext)
    {
        NS_ASSERTION(aProposed, "null resource");
        PRAutoLock _(mLock);

        PLHashEntry* second = *GetEntry(aProposed);
        OrderingEntry* e = static_cast<OrderingEntry*>(second->value);
        if (CallStack::kNone == e->mFirstSeen)
            e->mFirstSeen = aCallContext;

        if (!aLast)
            // don't check if |0 < proposed|; just vamoose
            return 0;

        PLHashEntry* first = *GetEntry(aLast);

        // this is the crux of the deadlock detector algorithm

        if (first == second) {
            // reflexive deadlock.  fastpath b/c InTransitiveClosure is
            // not applicable here.
            ResourceAcquisitionArray* cycle = new ResourceAcquisitionArray();
            if (!cycle)
                NS_RUNTIMEABORT("can't allocate dep. cycle array");
            cycle->AppendElement(MakeResourceAcquisition(first));
            cycle->AppendElement(ResourceAcquisition(aProposed,
                                                     aCallContext));
            return cycle;
        }
        if (InTransitiveClosure(first, second)) {
            // we've already established |last < proposed|.  all is well.
            return 0;
        }
        if (InTransitiveClosure(second, first)) {
            // the order |proposed < last| has been deduced, perhaps
            // transitively.  we're attempting to violate that
            // constraint by acquiring resources in the order
            // |last < proposed|, and thus we may deadlock under the
            // right conditions.
            ResourceAcquisitionArray* cycle = GetDeductionChain(second, first);
            // show how acquiring |proposed| would complete the cycle
            cycle->AppendElement(ResourceAcquisition(aProposed,
                                                     aCallContext));
            return cycle;
        }
        // |last|, |proposed| are unordered according to our
        // poset.  this is fine, but we now need to add this
        // ordering constraint.
        AddOrder(first, second);
        return 0;
    }

    /**
     * Return true iff |aTarget| is in the transitive closure of |aStart|
     * over the ordering relation `<_this'.
     *
     * @precondition |aStart != aTarget|
     */
    bool InTransitiveClosure(const PLHashEntry* aStart,
                             const PLHashEntry* aTarget) const
    {
        if (IsOrdered(aStart, aTarget))
            return true;

        index_type i = 0;
        size_type len = NumOrders(aStart);
        for (const PLHashEntry* const* it = GetOrders(aStart);
             i < len; ++i, ++it)
            if (InTransitiveClosure(*it, aTarget))
                return true;
        return false;
    }

    /**
     * Return an array of all resource acquisitions
     *   aStart <_this r1 <_this r2 <_ ... <_ aTarget
     * from which |aStart <_this aTarget| was deduced, including
     * |aStart| and |aTarget|.
     *
     * Nb: there may be multiple deductions of |aStart <_this
     * aTarget|.  This function returns the first ordering found by
     * depth-first search.
     *
     * Nb: |InTransitiveClosure| could be replaced by this function.
     * However, this one is more expensive because we record the DFS
     * search stack on the heap whereas the other doesn't.
     *
     * @precondition |aStart != aTarget|
     */
    ResourceAcquisitionArray* GetDeductionChain(
        const PLHashEntry* aStart,
        const PLHashEntry* aTarget)
    {
        ResourceAcquisitionArray* chain = new ResourceAcquisitionArray();
        if (!chain)
            NS_RUNTIMEABORT("can't allocate dep. cycle array");
        chain->AppendElement(MakeResourceAcquisition(aStart));

        NS_ASSERTION(GetDeductionChain_Helper(aStart, aTarget, chain),
                     "GetDeductionChain called when there's no deadlock");
        return chain;
    }

    // precondition: |aStart != aTarget|
    // invariant: |aStart| is the last element in |aChain|
    bool GetDeductionChain_Helper(const PLHashEntry* aStart,
                                  const PLHashEntry* aTarget,
                                  ResourceAcquisitionArray* aChain)
    {
        if (IsOrdered(aStart, aTarget)) {
            aChain->AppendElement(MakeResourceAcquisition(aTarget));
            return true;
        }

        index_type i = 0;
        size_type len = NumOrders(aStart);
        for (const PLHashEntry* const* it = GetOrders(aStart);
             i < len; ++i, ++it) {
            aChain->AppendElement(MakeResourceAcquisition(*it));
            if (GetDeductionChain_Helper(*it, aTarget, aChain))
                return true;
            aChain->RemoveElementAt(aChain->Length() - 1);
        }
        return false;
    }

    /**
     * The partial order on resource acquisitions used by the deadlock
     * detector.
     */
    PLHashTable* mOrdering;     // T* -> PLHashEntry<OrderingEntry>

    /**
     * Protects contentious methods.
     * Nb: can't use mozilla::Mutex since we are used as its deadlock
     * detector.
     */
    PRLock* mLock;

private:
    DeadlockDetector(const DeadlockDetector& aDD) MOZ_DELETE;
    DeadlockDetector& operator=(const DeadlockDetector& aDD) MOZ_DELETE;
};


template<typename T>
const PLHashAllocOps DeadlockDetector<T>::kAllocOps = {
    DeadlockDetector<T>::TableAlloc, DeadlockDetector<T>::TableFree,
    DeadlockDetector<T>::EntryAlloc, DeadlockDetector<T>::EntryFree
};


template<typename T>
// FIXME bug 456272: tune based on average workload
const uint32_t DeadlockDetector<T>::kDefaultNumBuckets = 64;


} // namespace mozilla

#endif // ifndef mozilla_DeadlockDetector_h
