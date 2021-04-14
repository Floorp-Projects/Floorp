/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BlockingResourceBase_h
#define mozilla_BlockingResourceBase_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/ThreadLocal.h"

#include "nscore.h"
#include "nsDebug.h"

#include "prtypes.h"

#ifdef DEBUG

// NB: Comment this out to enable callstack tracking.
#  define MOZ_CALLSTACK_DISABLED

#  include "prinit.h"

#  ifndef MOZ_CALLSTACK_DISABLED
#    include "mozilla/Maybe.h"
#    include "nsTArray.h"
#  endif

#endif

//
// This header is not meant to be included by client code.
//

namespace mozilla {

#ifdef DEBUG
template <class T>
class DeadlockDetector;
#endif

/**
 * BlockingResourceBase
 * Base class of resources that might block clients trying to acquire them.
 * Does debugging and deadlock detection in DEBUG builds.
 **/
class BlockingResourceBase {
 public:
  // Needs to be kept in sync with kResourceTypeNames.
  enum BlockingResourceType {
    eMutex,
    eReentrantMonitor,
    eCondVar,
    eRecursiveMutex
  };

  /**
   * kResourceTypeName
   * Human-readable version of BlockingResourceType enum.
   */
  static const char* const kResourceTypeName[];

#ifdef DEBUG

  static size_t SizeOfDeadlockDetector(MallocSizeOf aMallocSizeOf);

  /**
   * Print
   * Write a description of this blocking resource to |aOut|.  If
   * the resource appears to be currently acquired, the current
   * acquisition context is printed and true is returned.
   * Otherwise, we print the context from |aFirstSeen|, the
   * first acquisition from which the code calling |Print()|
   * became interested in us, and return false.
   *
   * *NOT* thread safe.  Reads |mAcquisitionContext| without
   * synchronization, but this will not cause correctness
   * problems.
   *
   * FIXME bug 456272: hack alert: because we can't write call
   * contexts into strings, all info is written to stderr, but
   * only some info is written into |aOut|
   */
  bool Print(nsACString& aOut) const;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    // NB: |mName| is not reported as it's expected to be a static string.
    //     If we switch to a nsString it should be added to the tally.
    //     |mChainPrev| is not reported because its memory is not owned.
    size_t n = aMallocSizeOf(this);
    return n;
  }

  // ``DDT'' = ``Deadlock Detector Type''
  typedef DeadlockDetector<BlockingResourceBase> DDT;

 protected:
#  ifdef MOZ_CALLSTACK_DISABLED
  typedef bool AcquisitionState;
#  else
  // Using maybe to use emplacement as the acquisition state flag; we may not
  // always get a stack trace because of possible stack walk suppression or
  // errors, hence can't use !IsEmpty() on the array itself as indication.
  static size_t const kAcquisitionStateStackSize = 24;
  typedef Maybe<AutoTArray<void*, kAcquisitionStateStackSize> >
      AcquisitionState;
#  endif

  /**
   * BlockingResourceBase
   * Initialize this blocking resource.  Also hooks the resource into
   * instrumentation code.
   *
   * Thread safe.
   *
   * @param aName A meaningful, unique name that can be used in
   *              error messages, et al.
   * @param aType The specific type of |this|, if any.
   **/
  BlockingResourceBase(const char* aName, BlockingResourceType aType);

  ~BlockingResourceBase();

  /**
   * CheckAcquire
   *
   * Thread safe.
   **/
  void CheckAcquire();

  /**
   * Acquire
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   **/
  void Acquire();  // NS_NEEDS_RESOURCE(this)

  /**
   * Release
   * Remove this resource from the current thread's acquisition chain.
   * The resource does not have to be at the front of the chain, although
   * it is confusing to release resources in a different order than they
   * are acquired.  This generates a warning.
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   **/
  void Release();  // NS_NEEDS_RESOURCE(this)

  /**
   * ResourceChainFront
   *
   * Thread safe.
   *
   * @return the front of the resource acquisition chain, i.e., the last
   *         resource acquired.
   */
  static BlockingResourceBase* ResourceChainFront() {
    return sResourceAcqnChainFront.get();
  }

  /**
   * ResourceChainPrev
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   */
  static BlockingResourceBase* ResourceChainPrev(
      const BlockingResourceBase* aResource) {
    return aResource->mChainPrev;
  }  // NS_NEEDS_RESOURCE(this)

  /**
   * ResourceChainAppend
   * Set |this| to the front of the resource acquisition chain, and link
   * |this| to |aPrev|.
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   */
  void ResourceChainAppend(BlockingResourceBase* aPrev) {
    mChainPrev = aPrev;
    sResourceAcqnChainFront.set(this);
  }  // NS_NEEDS_RESOURCE(this)

  /**
   * ResourceChainRemove
   * Remove |this| from the front of the resource acquisition chain.
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   */
  void ResourceChainRemove() {
    NS_ASSERTION(this == ResourceChainFront(), "not at chain front");
    sResourceAcqnChainFront.set(mChainPrev);
  }  // NS_NEEDS_RESOURCE(this)

  /**
   * TakeAcquisitionState
   * Return whether or not this resource was acquired and mark the resource
   * as not acquired for subsequent uses.
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   */
  AcquisitionState TakeAcquisitionState() {
#  ifdef MOZ_CALLSTACK_DISABLED
    bool acquired = mAcquired;
    ClearAcquisitionState();
    return acquired;
#  else
    return mAcquired.take();
#  endif
  }

  /**
   * SetAcquisitionState
   * Set whether or not this resource was acquired.
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   */
  void SetAcquisitionState(AcquisitionState&& aAcquisitionState) {
    mAcquired = std::move(aAcquisitionState);
  }

  /**
   * ClearAcquisitionState
   * Indicate this resource is not acquired.
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   */
  void ClearAcquisitionState() {
#  ifdef MOZ_CALLSTACK_DISABLED
    mAcquired = false;
#  else
    mAcquired.reset();
#  endif
  }

  /**
   * IsAcquired
   * Indicates if this resource is acquired.
   *
   * *NOT* thread safe.  Requires ownership of underlying resource.
   */
  bool IsAcquired() const { return (bool)mAcquired; }

  /**
   * mChainPrev
   * A series of resource acquisitions creates a chain of orders.  This
   * chain is implemented as a linked list; |mChainPrev| points to the
   * resource most recently Acquire()'d before this one.
   **/
  BlockingResourceBase* mChainPrev;

 private:
  /**
   * mName
   * A descriptive name for this resource.  Used in error
   * messages etc.
   */
  const char* mName;

  /**
   * mType
   * The more specific type of this resource.  Used to implement
   * special semantics (e.g., reentrancy of monitors).
   **/
  BlockingResourceType mType;

  /**
   * mAcquired
   * Indicates if this resource is currently acquired.
   */
  AcquisitionState mAcquired;

#  ifndef MOZ_CALLSTACK_DISABLED
  /**
   * mFirstSeen
   * Inidicates where this resource was first acquired.
   */
  AcquisitionState mFirstSeen;
#  endif

  /**
   * sCallOnce
   * Ensures static members are initialized only once, and in a
   * thread-safe way.
   */
  static PRCallOnceType sCallOnce;

  /**
   * Thread-private pointer to the front of each thread's resource
   * acquisition chain.
   */
  static MOZ_THREAD_LOCAL(BlockingResourceBase*) sResourceAcqnChainFront;

  /**
   * sDeadlockDetector
   * Does as named.
   */
  static DDT* sDeadlockDetector;

  /**
   * InitStatics
   * Inititialize static members of BlockingResourceBase that can't
   * be statically initialized.
   *
   * *NOT* thread safe.
   */
  static PRStatus InitStatics();

  /**
   * Shutdown
   * Free static members.
   *
   * *NOT* thread safe.
   */
  static void Shutdown();

  static void StackWalkCallback(uint32_t aFrameNumber, void* aPc, void* aSp,
                                void* aClosure);
  static void GetStackTrace(AcquisitionState& aState,
                            const void* aFirstFramePC);

#  ifdef MOZILLA_INTERNAL_API
  // so it can call BlockingResourceBase::Shutdown()
  friend void LogTerm();
#  endif  // ifdef MOZILLA_INTERNAL_API

#else  // non-DEBUG implementation

  BlockingResourceBase(const char* aName, BlockingResourceType aType) {}

  ~BlockingResourceBase() {}

#endif
};

}  // namespace mozilla

#endif  // mozilla_BlockingResourceBase_h
