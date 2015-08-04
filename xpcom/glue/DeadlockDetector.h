/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_DeadlockDetector_h
#define mozilla_DeadlockDetector_h

#include "mozilla/Attributes.h"

#include <stdlib.h>

#include "prlock.h"

#include "nsClassHashtable.h"
#include "nsTArray.h"

namespace mozilla {

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
template<typename T>
class DeadlockDetector
{
public:
  typedef nsTArray<const T*> ResourceAcquisitionArray;

private:
  struct OrderingEntry;
  typedef nsTArray<OrderingEntry*> HashEntryArray;
  typedef typename HashEntryArray::index_type index_type;
  typedef typename HashEntryArray::size_type size_type;
  static const index_type NoIndex = HashEntryArray::NoIndex;

  /**
   * Value type for the ordering table.  Contains the other
   * resources on which an ordering constraint |key < other|
   * exists.  The catch is that we also store the calling context at
   * which the other resource was acquired; this improves the
   * quality of error messages when potential deadlock is detected.
   */
  struct OrderingEntry
  {
    explicit OrderingEntry(const T* aResource)
      : mOrderedLT()        // FIXME bug 456272: set to empirical dep size?
      , mExternalRefs()
      , mResource(aResource)
    {
    }
    ~OrderingEntry()
    {
    }

    size_t
    SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
    {
      size_t n = aMallocSizeOf(this);
      n += mOrderedLT.ShallowSizeOfExcludingThis(aMallocSizeOf);
      n += mExternalRefs.ShallowSizeOfExcludingThis(aMallocSizeOf);
      return n;
    }

    HashEntryArray mOrderedLT; // this <_o Other
    HashEntryArray mExternalRefs; // hash entries that reference this
    const T* mResource;
  };

  // Throwaway RAII lock to make the following code safer.
  struct PRAutoLock
  {
    explicit PRAutoLock(PRLock* aLock) : mLock(aLock) { PR_Lock(mLock); }
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
  explicit DeadlockDetector(uint32_t aNumResourcesGuess = kDefaultNumBuckets)
    : mOrdering(aNumResourcesGuess)
  {
    mLock = PR_NewLock();
    if (!mLock) {
      NS_RUNTIMEABORT("couldn't allocate deadlock detector lock");
    }
  }

  /**
   * ~DeadlockDetector
   *
   * *NOT* thread safe.
   */
  ~DeadlockDetector()
  {
    PR_DestroyLock(mLock);
  }

  size_t
  SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t n = aMallocSizeOf(this);

    {
      PRAutoLock _(mLock);
      n += mOrdering.ShallowSizeOfExcludingThis(aMallocSizeOf);
      for (auto iter = mOrdering.ConstIter(); !iter.Done(); iter.Next()) {
        // NB: Key is accounted for in the entry.
        n += iter.Data()->SizeOfIncludingThis(aMallocSizeOf);
      }
    }

    return n;
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
  void Add(const T* aResource)
  {
    PRAutoLock _(mLock);
    mOrdering.Put(aResource, new OrderingEntry(aResource));
  }

  void Remove(const T* aResource)
  {
    PRAutoLock _(mLock);

    OrderingEntry* entry = mOrdering.Get(aResource);

    // Iterate the external refs and remove the entry from them.
    HashEntryArray& refs = entry->mExternalRefs;
    for (index_type i = 0; i < refs.Length(); i++) {
      refs[i]->mOrderedLT.RemoveElementSorted(entry);
    }

    // Iterate orders and remove this entry from their refs.
    HashEntryArray& orders = entry->mOrderedLT;
    for (index_type i = 0; i < orders.Length(); i++) {
      orders[i]->mExternalRefs.RemoveElementSorted(entry);
    }

    // Now the entry can be safely removed.
    mOrdering.Remove(aResource);
  }

  /**
   * CheckAcquisition This method is called after acquiring |aLast|,
   * but before trying to acquire |aProposed|.
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
   */
  ResourceAcquisitionArray* CheckAcquisition(const T* aLast,
                                             const T* aProposed)
  {
    if (!aLast) {
      // don't check if |0 < aProposed|; just vamoose
      return 0;
    }

    NS_ASSERTION(aProposed, "null resource");
    PRAutoLock _(mLock);

    OrderingEntry* proposed = mOrdering.Get(aProposed);
    NS_ASSERTION(proposed, "missing ordering entry");

    OrderingEntry* current = mOrdering.Get(aLast);
    NS_ASSERTION(current, "missing ordering entry");

    // this is the crux of the deadlock detector algorithm

    if (current == proposed) {
      // reflexive deadlock.  fastpath b/c InTransitiveClosure is
      // not applicable here.
      ResourceAcquisitionArray* cycle = new ResourceAcquisitionArray();
      if (!cycle) {
        NS_RUNTIMEABORT("can't allocate dep. cycle array");
      }
      cycle->AppendElement(current->mResource);
      cycle->AppendElement(aProposed);
      return cycle;
    }
    if (InTransitiveClosure(current, proposed)) {
      // we've already established |aLast < aProposed|.  all is well.
      return 0;
    }
    if (InTransitiveClosure(proposed, current)) {
      // the order |aProposed < aLast| has been deduced, perhaps
      // transitively.  we're attempting to violate that
      // constraint by acquiring resources in the order
      // |aLast < aProposed|, and thus we may deadlock under the
      // right conditions.
      ResourceAcquisitionArray* cycle = GetDeductionChain(proposed, current);
      // show how acquiring |aProposed| would complete the cycle
      cycle->AppendElement(aProposed);
      return cycle;
    }
    // |aLast|, |aProposed| are unordered according to our
    // poset.  this is fine, but we now need to add this
    // ordering constraint.
    current->mOrderedLT.InsertElementSorted(proposed);
    proposed->mExternalRefs.InsertElementSorted(current);
    return 0;
  }

  /**
   * Return true iff |aTarget| is in the transitive closure of |aStart|
   * over the ordering relation `<_this'.
   *
   * @precondition |aStart != aTarget|
   */
  bool InTransitiveClosure(const OrderingEntry* aStart,
                           const OrderingEntry* aTarget) const
  {
    // NB: Using a static comparator rather than default constructing one shows
    //     a 9% improvement in scalability tests on some systems.
    static nsDefaultComparator<const OrderingEntry*, const OrderingEntry*> comp;
    if (aStart->mOrderedLT.BinaryIndexOf(aTarget, comp) != NoIndex) {
      return true;
    }

    index_type i = 0;
    size_type len = aStart->mOrderedLT.Length();
    for (auto it = aStart->mOrderedLT.Elements(); i < len; ++i, ++it) {
      if (InTransitiveClosure(*it, aTarget)) {
        return true;
      }
    }
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
  ResourceAcquisitionArray* GetDeductionChain(const OrderingEntry* aStart,
                                              const OrderingEntry* aTarget)
  {
    ResourceAcquisitionArray* chain = new ResourceAcquisitionArray();
    if (!chain) {
      NS_RUNTIMEABORT("can't allocate dep. cycle array");
    }
    chain->AppendElement(aStart->mResource);

    NS_ASSERTION(GetDeductionChain_Helper(aStart, aTarget, chain),
                 "GetDeductionChain called when there's no deadlock");
    return chain;
  }

  // precondition: |aStart != aTarget|
  // invariant: |aStart| is the last element in |aChain|
  bool GetDeductionChain_Helper(const OrderingEntry* aStart,
                                const OrderingEntry* aTarget,
                                ResourceAcquisitionArray* aChain)
  {
    if (aStart->mOrderedLT.BinaryIndexOf(aTarget) != NoIndex) {
      aChain->AppendElement(aTarget->mResource);
      return true;
    }

    index_type i = 0;
    size_type len = aStart->mOrderedLT.Length();
    for (auto it = aStart->mOrderedLT.Elements(); i < len; ++i, ++it) {
      aChain->AppendElement((*it)->mResource);
      if (GetDeductionChain_Helper(*it, aTarget, aChain)) {
        return true;
      }
      aChain->RemoveElementAt(aChain->Length() - 1);
    }
    return false;
  }

  /**
   * The partial order on resource acquisitions used by the deadlock
   * detector.
   */
  nsClassHashtable<nsPtrHashKey<const T>, OrderingEntry> mOrdering;


  /**
   * Protects contentious methods.
   * Nb: can't use mozilla::Mutex since we are used as its deadlock
   * detector.
   */
  PRLock* mLock;

private:
  DeadlockDetector(const DeadlockDetector& aDD) = delete;
  DeadlockDetector& operator=(const DeadlockDetector& aDD) = delete;
};


template<typename T>
// FIXME bug 456272: tune based on average workload
const uint32_t DeadlockDetector<T>::kDefaultNumBuckets = 32;


} // namespace mozilla

#endif // ifndef mozilla_DeadlockDetector_h
