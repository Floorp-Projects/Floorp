/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsISupports.h"


#ifndef nsISupportsImpl_h__
#define nsISupportsImpl_h__

#include "nscore.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"


#if !defined(XPCOM_GLUE_AVOID_NSPR)
#include "prthread.h" /* needed for thread-safety checks */
#endif // !XPCOM_GLUE_AVOID_NSPR

#include "nsDebug.h"
#include "nsXPCOM.h"
#ifndef XPCOM_GLUE
#include "mozilla/Atomics.h"
#endif
#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"
#include "mozilla/MacroArgs.h"
#include "mozilla/MacroForEach.h"

inline nsISupports*
ToSupports(nsISupports* p)
{
    return p;
}

inline nsISupports*
ToCanonicalSupports(nsISupports* p)
{
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Macros to help detect thread-safety:

#if (defined(DEBUG) || (defined(NIGHTLY_BUILD) && !defined(MOZ_PROFILING))) && !defined(XPCOM_GLUE_AVOID_NSPR)

class nsAutoOwningThread {
public:
    nsAutoOwningThread() { mThread = PR_GetCurrentThread(); }
    void *GetThread() const { return mThread; }

private:
    void *mThread;
};

#define NS_DECL_OWNINGTHREAD            nsAutoOwningThread _mOwningThread;
#define NS_ASSERT_OWNINGTHREAD_AGGREGATE(agg, _class) \
  NS_CheckThreadSafe(agg->_mOwningThread.GetThread(), #_class " not thread-safe")
#define NS_ASSERT_OWNINGTHREAD(_class) NS_ASSERT_OWNINGTHREAD_AGGREGATE(this, _class)
#else // !DEBUG && !(NIGHTLY_BUILD && !MOZ_PROFILING)

#define NS_DECL_OWNINGTHREAD            /* nothing */
#define NS_ASSERT_OWNINGTHREAD_AGGREGATE(agg, _class) ((void)0)
#define NS_ASSERT_OWNINGTHREAD(_class)  ((void)0)

#endif // DEBUG || (NIGHTLY_BUILD && !MOZ_PROFILING)


// Macros for reference-count and constructor logging

#ifdef NS_BUILD_REFCNT_LOGGING

#define NS_LOG_ADDREF(_p, _rc, _type, _size) \
  NS_LogAddRef((_p), (_rc), (_type), (uint32_t) (_size))

#define NS_LOG_RELEASE(_p, _rc, _type) \
  NS_LogRelease((_p), (_rc), (_type))

// Note that the following constructor/destructor logging macros are redundant
// for refcounted objects that log via the NS_LOG_ADDREF/NS_LOG_RELEASE macros.
// Refcount logging is preferred.
#define MOZ_COUNT_CTOR(_type)                                 \
do {                                                          \
  NS_LogCtor((void*)this, #_type, sizeof(*this));             \
} while (0)

#define MOZ_COUNT_CTOR_INHERITED(_type, _base)                    \
do {                                                              \
  NS_LogCtor((void*)this, #_type, sizeof(*this) - sizeof(_base)); \
} while (0)

#define MOZ_COUNT_DTOR(_type)                                 \
do {                                                          \
  NS_LogDtor((void*)this, #_type, sizeof(*this));             \
} while (0)

#define MOZ_COUNT_DTOR_INHERITED(_type, _base)                    \
do {                                                              \
  NS_LogDtor((void*)this, #_type, sizeof(*this) - sizeof(_base)); \
} while (0)

/* nsCOMPtr.h allows these macros to be defined by clients
 * These logging functions require dynamic_cast<void*>, so they don't
 * do anything useful if we don't have dynamic_cast<void*>. */
#define NSCAP_LOG_ASSIGNMENT(_c, _p)                                \
  if (_p)                                                           \
    NS_LogCOMPtrAddRef((_c),static_cast<nsISupports*>(_p))

#define NSCAP_LOG_RELEASE(_c, _p)                                   \
  if (_p)                                                           \
    NS_LogCOMPtrRelease((_c), static_cast<nsISupports*>(_p))

#else /* !NS_BUILD_REFCNT_LOGGING */

#define NS_LOG_ADDREF(_p, _rc, _type, _size)
#define NS_LOG_RELEASE(_p, _rc, _type)
#define MOZ_COUNT_CTOR(_type)
#define MOZ_COUNT_CTOR_INHERITED(_type, _base)
#define MOZ_COUNT_DTOR(_type)
#define MOZ_COUNT_DTOR_INHERITED(_type, _base)

#endif /* NS_BUILD_REFCNT_LOGGING */


// Support for ISupports classes which interact with cycle collector.

#define NS_NUMBER_OF_FLAGS_IN_REFCNT 2
#define NS_IN_PURPLE_BUFFER (1 << 0)
#define NS_IS_PURPLE (1 << 1)
#define NS_REFCOUNT_CHANGE (1 << NS_NUMBER_OF_FLAGS_IN_REFCNT)
#define NS_REFCOUNT_VALUE(_val) (_val >> NS_NUMBER_OF_FLAGS_IN_REFCNT)

class nsCycleCollectingAutoRefCnt {

public:
  nsCycleCollectingAutoRefCnt()
    : mRefCntAndFlags(0)
  {}

  explicit nsCycleCollectingAutoRefCnt(uintptr_t aValue)
    : mRefCntAndFlags(aValue << NS_NUMBER_OF_FLAGS_IN_REFCNT)
  {
  }

  MOZ_ALWAYS_INLINE uintptr_t incr(nsISupports *owner)
  {
    return incr(owner, nullptr);
  }

  MOZ_ALWAYS_INLINE uintptr_t incr(void *owner, nsCycleCollectionParticipant *p)
  {
    mRefCntAndFlags += NS_REFCOUNT_CHANGE;
    mRefCntAndFlags &= ~NS_IS_PURPLE;
    // For incremental cycle collection, use the purple buffer to track objects
    // that have been AddRef'd.
    if (!IsInPurpleBuffer()) {
      mRefCntAndFlags |= NS_IN_PURPLE_BUFFER;
      // Refcount isn't zero, so Suspect won't delete anything.
      MOZ_ASSERT(get() > 0);
      NS_CycleCollectorSuspect3(owner, p, this, nullptr);
    }
    return NS_REFCOUNT_VALUE(mRefCntAndFlags);
  }

  MOZ_ALWAYS_INLINE void stabilizeForDeletion()
  {
    // Set refcnt to 1 and mark us to be in the purple buffer.
    // This way decr won't call suspect again.
    mRefCntAndFlags = NS_REFCOUNT_CHANGE | NS_IN_PURPLE_BUFFER;
  }

  MOZ_ALWAYS_INLINE uintptr_t decr(nsISupports *owner,
                                   bool *shouldDelete = nullptr)
  {
    return decr(owner, nullptr, shouldDelete);
  }

  MOZ_ALWAYS_INLINE uintptr_t decr(void *owner, nsCycleCollectionParticipant *p,
                                   bool *shouldDelete = nullptr)
  {
    MOZ_ASSERT(get() > 0);
    if (!IsInPurpleBuffer()) {
      mRefCntAndFlags -= NS_REFCOUNT_CHANGE;
      mRefCntAndFlags |= (NS_IN_PURPLE_BUFFER | NS_IS_PURPLE);
      uintptr_t retval = NS_REFCOUNT_VALUE(mRefCntAndFlags);
      // Suspect may delete 'owner' and 'this'!
      NS_CycleCollectorSuspect3(owner, p, this, shouldDelete);
      return retval;
    }
    mRefCntAndFlags -= NS_REFCOUNT_CHANGE;
    mRefCntAndFlags |= (NS_IN_PURPLE_BUFFER | NS_IS_PURPLE);
    return NS_REFCOUNT_VALUE(mRefCntAndFlags);
  }

  MOZ_ALWAYS_INLINE void RemovePurple()
  {
    MOZ_ASSERT(IsPurple(), "must be purple");
    mRefCntAndFlags &= ~NS_IS_PURPLE;
  }

  MOZ_ALWAYS_INLINE void RemoveFromPurpleBuffer()
  {
    MOZ_ASSERT(IsInPurpleBuffer());
    mRefCntAndFlags &= ~(NS_IS_PURPLE | NS_IN_PURPLE_BUFFER);
  }

  MOZ_ALWAYS_INLINE bool IsPurple() const
  {
    return !!(mRefCntAndFlags & NS_IS_PURPLE);
  }

  MOZ_ALWAYS_INLINE bool IsInPurpleBuffer() const
  {
    return !!(mRefCntAndFlags & NS_IN_PURPLE_BUFFER);
  }

  MOZ_ALWAYS_INLINE nsrefcnt get() const
  {
    return NS_REFCOUNT_VALUE(mRefCntAndFlags);
  }

  MOZ_ALWAYS_INLINE operator nsrefcnt() const
  {
    return get();
  }

 private:
  uintptr_t mRefCntAndFlags;
};

class nsAutoRefCnt {

 public:
    nsAutoRefCnt() : mValue(0) {}
    explicit nsAutoRefCnt(nsrefcnt aValue) : mValue(aValue) {}

    // only support prefix increment/decrement
    nsrefcnt operator++() { return ++mValue; }
    nsrefcnt operator--() { return --mValue; }

    nsrefcnt operator=(nsrefcnt aValue) { return (mValue = aValue); }
    operator nsrefcnt() const { return mValue; }
    nsrefcnt get() const { return mValue; }

    static const bool isThreadSafe = false;
 private:
    nsrefcnt operator++(int) MOZ_DELETE;
    nsrefcnt operator--(int) MOZ_DELETE;
    nsrefcnt mValue;
};

#ifndef XPCOM_GLUE
namespace mozilla {
class ThreadSafeAutoRefCnt {
 public:
    ThreadSafeAutoRefCnt() : mValue(0) {}
    explicit ThreadSafeAutoRefCnt(nsrefcnt aValue) : mValue(aValue) {}
    
    // only support prefix increment/decrement
    MOZ_ALWAYS_INLINE nsrefcnt operator++() { return ++mValue; }
    MOZ_ALWAYS_INLINE nsrefcnt operator--() { return --mValue; }

    MOZ_ALWAYS_INLINE nsrefcnt operator=(nsrefcnt aValue) { return (mValue = aValue); }
    MOZ_ALWAYS_INLINE operator nsrefcnt() const { return mValue; }
    MOZ_ALWAYS_INLINE nsrefcnt get() const { return mValue; }

    static const bool isThreadSafe = true;
 private:
    nsrefcnt operator++(int) MOZ_DELETE;
    nsrefcnt operator--(int) MOZ_DELETE;
    // In theory, RelaseAcquire consistency (but no weaker) is sufficient for
    // the counter. Making it weaker could speed up builds on ARM (but not x86),
    // but could break pre-existing code that assumes sequential consistency.
    Atomic<nsrefcnt> mValue;
};
}
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * Declare the reference count variable and the implementations of the
 * AddRef and QueryInterface methods.
 */

#define NS_DECL_ISUPPORTS                                                     \
public:                                                                       \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);                          \
  NS_IMETHOD_(MozExternalRefCountType) Release(void);                         \
protected:                                                                    \
  nsAutoRefCnt mRefCnt;                                                       \
  NS_DECL_OWNINGTHREAD                                                        \
public:

#define NS_DECL_THREADSAFE_ISUPPORTS                                          \
public:                                                                       \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);                          \
  NS_IMETHOD_(MozExternalRefCountType) Release(void);                         \
protected:                                                                    \
  ::mozilla::ThreadSafeAutoRefCnt mRefCnt;                                    \
  NS_DECL_OWNINGTHREAD                                                        \
public:

#define NS_DECL_CYCLE_COLLECTING_ISUPPORTS                                    \
public:                                                                       \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);                          \
  NS_IMETHOD_(MozExternalRefCountType) Release(void);                         \
  NS_IMETHOD_(void) DeleteCycleCollectable(void);                             \
protected:                                                                    \
  nsCycleCollectingAutoRefCnt mRefCnt;                                        \
  NS_DECL_OWNINGTHREAD                                                        \
public:


///////////////////////////////////////////////////////////////////////////////

/*
 * Implementation of AddRef and Release for non-nsISupports (ie "native")
 * cycle-collected classes that use the purple buffer to avoid leaks.
 */

#define NS_IMPL_CC_NATIVE_ADDREF_BODY(_class)                                 \
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                      \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
    nsrefcnt count =                                                          \
      mRefCnt.incr(static_cast<void*>(this),                                  \
                   _class::NS_CYCLE_COLLECTION_INNERCLASS::GetParticipant()); \
    NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                       \
    return count;

#define NS_IMPL_CC_NATIVE_RELEASE_BODY(_class)                                \
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                          \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
    nsrefcnt count =                                                          \
      mRefCnt.decr(static_cast<void*>(this),                                  \
                   _class::NS_CYCLE_COLLECTION_INNERCLASS::GetParticipant()); \
    NS_LOG_RELEASE(this, count, #_class);                                     \
    return count;

#define NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(_class)                        \
NS_METHOD_(MozExternalRefCountType) _class::AddRef(void)                      \
{                                                                             \
  NS_IMPL_CC_NATIVE_ADDREF_BODY(_class)                                       \
}

#define NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE_WITH_LAST_RELEASE(_class, _last) \
NS_METHOD_(MozExternalRefCountType) _class::Release(void)                        \
{                                                                                \
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                             \
    NS_ASSERT_OWNINGTHREAD(_class);                                              \
    bool shouldDelete = false;                                                   \
    nsrefcnt count =                                                             \
      mRefCnt.decr(static_cast<void*>(this),                                     \
                   _class::NS_CYCLE_COLLECTION_INNERCLASS::GetParticipant(),     \
                   &shouldDelete);                                               \
    NS_LOG_RELEASE(this, count, #_class);                                        \
    if (count == 0) {                                                            \
        mRefCnt.incr(static_cast<void*>(this),                                   \
                     _class::NS_CYCLE_COLLECTION_INNERCLASS::GetParticipant());  \
        _last;                                                                   \
        mRefCnt.decr(static_cast<void*>(this),                                   \
                     _class::NS_CYCLE_COLLECTION_INNERCLASS::GetParticipant());  \
        if (shouldDelete) {                                                      \
            mRefCnt.stabilizeForDeletion();                                      \
            DeleteCycleCollectable();                                            \
        }                                                                        \
    }                                                                            \
    return count;                                                                \
}

#define NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE(_class)                       \
NS_METHOD_(MozExternalRefCountType) _class::Release(void)                     \
{                                                                             \
  NS_IMPL_CC_NATIVE_RELEASE_BODY(_class)                                      \
}

#define NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(_class)            \
public:                                                                       \
  NS_METHOD_(MozExternalRefCountType) AddRef(void) {                          \
    NS_IMPL_CC_NATIVE_ADDREF_BODY(_class)                                     \
  }                                                                           \
  NS_METHOD_(MozExternalRefCountType) Release(void) {                         \
    NS_IMPL_CC_NATIVE_RELEASE_BODY(_class)                                    \
  }                                                                           \
protected:                                                                    \
  nsCycleCollectingAutoRefCnt mRefCnt;                                        \
  NS_DECL_OWNINGTHREAD                                                        \
public:


///////////////////////////////////////////////////////////////////////////////

/**
 * Previously used to initialize the reference count, but no longer needed.
 *
 * DEPRECATED.
 */
#define NS_INIT_ISUPPORTS() ((void)0)

/**
 * Use this macro to declare and implement the AddRef & Release methods for a
 * given non-XPCOM <i>_class</i>.
 *
 * @param _class The name of the class implementing the method
 */
#define NS_INLINE_DECL_REFCOUNTING(_class)                                    \
public:                                                                       \
  NS_METHOD_(MozExternalRefCountType) AddRef(void) {                          \
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                      \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
    ++mRefCnt;                                                                \
    NS_LOG_ADDREF(this, mRefCnt, #_class, sizeof(*this));                     \
    return mRefCnt;                                                           \
  }                                                                           \
  NS_METHOD_(MozExternalRefCountType) Release(void) {                         \
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                          \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
    --mRefCnt;                                                                \
    NS_LOG_RELEASE(this, mRefCnt, #_class);                                   \
    if (mRefCnt == 0) {                                                       \
      NS_ASSERT_OWNINGTHREAD(_class);                                         \
      mRefCnt = 1; /* stabilize */                                            \
      delete this;                                                            \
      return 0;                                                               \
    }                                                                         \
    return mRefCnt;                                                           \
  }                                                                           \
protected:                                                                    \
  nsAutoRefCnt mRefCnt;                                                       \
  NS_DECL_OWNINGTHREAD                                                        \
public:

/**
 * Use this macro to declare and implement the AddRef & Release methods for a
 * given non-XPCOM <i>_class</i> in a threadsafe manner.
 *
 * DOES NOT DO REFCOUNT STABILIZATION!
 *
 * @param _class The name of the class implementing the method
 */
#define NS_INLINE_DECL_THREADSAFE_REFCOUNTING(_class)                         \
public:                                                                       \
  NS_METHOD_(MozExternalRefCountType) AddRef(void) {                          \
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                      \
    nsrefcnt count = ++mRefCnt;                                               \
    NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                       \
    return (nsrefcnt) count;                                                  \
  }                                                                           \
  NS_METHOD_(MozExternalRefCountType) Release(void) {                         \
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                          \
    nsrefcnt count = --mRefCnt;                                               \
    NS_LOG_RELEASE(this, count, #_class);                                     \
    if (count == 0) {                                                         \
      delete (this);                                                          \
      return 0;                                                               \
    }                                                                         \
    return count;                                                             \
  }                                                                           \
protected:                                                                    \
  ::mozilla::ThreadSafeAutoRefCnt mRefCnt;                                    \
public:

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_ADDREF(_class)                                                \
NS_IMETHODIMP_(MozExternalRefCountType) _class::AddRef(void)                  \
{                                                                             \
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                        \
  if (!mRefCnt.isThreadSafe)                                                  \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
  nsrefcnt count = ++mRefCnt;                                                 \
  NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                         \
  return count;                                                               \
}

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * implemented as a wholly owned aggregated object intended to implement
 * interface(s) for its owner
 * @param _class The name of the class implementing the method
 * @param _aggregator the owning/containing object
 */
#define NS_IMPL_ADDREF_USING_AGGREGATOR(_class, _aggregator)                  \
NS_IMETHODIMP_(MozExternalRefCountType) _class::AddRef(void)                  \
{                                                                             \
  NS_PRECONDITION(_aggregator, "null aggregator");                            \
  return (_aggregator)->AddRef();                                             \
}

/**
 * Use this macro to implement the Release method for a given
 * <i>_class</i>.
 * @param _class The name of the class implementing the method
 * @param _destroy A statement that is executed when the object's
 *   refcount drops to zero.
 *
 * For example,
 *
 *   NS_IMPL_RELEASE_WITH_DESTROY(Foo, Destroy(this))
 *
 * will cause
 *
 *   Destroy(this);
 *
 * to be invoked when the object's refcount drops to zero. This
 * allows for arbitrary teardown activity to occur (e.g., deallocation
 * of object allocated with placement new).
 */
#define NS_IMPL_RELEASE_WITH_DESTROY(_class, _destroy)                        \
NS_IMETHODIMP_(MozExternalRefCountType) _class::Release(void)                 \
{                                                                             \
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                            \
  if (!mRefCnt.isThreadSafe)                                                  \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
  nsrefcnt count = --mRefCnt;                                                 \
  NS_LOG_RELEASE(this, count, #_class);                                       \
  if (count == 0) {                                                           \
    if (!mRefCnt.isThreadSafe)                                                \
      NS_ASSERT_OWNINGTHREAD(_class);                                         \
    mRefCnt = 1; /* stabilize */                                              \
    _destroy;                                                                 \
    return 0;                                                                 \
  }                                                                           \
  return count;                                                               \
}

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 *
 * A note on the 'stabilization' of the refcnt to one. At that point,
 * the object's refcount will have gone to zero. The object's
 * destructor may trigger code that attempts to QueryInterface() and
 * Release() 'this' again. Doing so will temporarily increment and
 * decrement the refcount. (Only a logic error would make one try to
 * keep a permanent hold on 'this'.)  To prevent re-entering the
 * destructor, we make sure that no balanced refcounting can return
 * the refcount to |0|.
 */
#define NS_IMPL_RELEASE(_class) \
  NS_IMPL_RELEASE_WITH_DESTROY(_class, delete (this))

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * implemented as a wholly owned aggregated object intended to implement
 * interface(s) for its owner
 * @param _class The name of the class implementing the method
 * @param _aggregator the owning/containing object
 */
#define NS_IMPL_RELEASE_USING_AGGREGATOR(_class, _aggregator)                 \
NS_IMETHODIMP_(MozExternalRefCountType) _class::Release(void)                 \
{                                                                             \
  NS_PRECONDITION(_aggregator, "null aggregator");                            \
  return (_aggregator)->Release();                                            \
}


#define NS_IMPL_CYCLE_COLLECTING_ADDREF(_class)                               \
NS_IMETHODIMP_(MozExternalRefCountType) _class::AddRef(void)                  \
{                                                                             \
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                        \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  nsISupports *base = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this);    \
  nsrefcnt count = mRefCnt.incr(base);                                        \
  NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                         \
  return count;                                                               \
}

#define NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(_class, _destroy)       \
NS_IMETHODIMP_(MozExternalRefCountType) _class::Release(void)                 \
{                                                                             \
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                            \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  nsISupports *base = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this);    \
  nsrefcnt count = mRefCnt.decr(base);                                        \
  NS_LOG_RELEASE(this, count, #_class);                                       \
  return count;                                                               \
}                                                                             \
NS_IMETHODIMP_(void) _class::DeleteCycleCollectable(void)                     \
{                                                                             \
  _destroy;                                                                   \
}

#define NS_IMPL_CYCLE_COLLECTING_RELEASE(_class)                              \
  NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(_class, delete (this))

// _LAST_RELEASE can be useful when certain resources should be released
// as soon as we know the object will be deleted.
#define NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(_class, _last)     \
NS_IMETHODIMP_(MozExternalRefCountType) _class::Release(void)                 \
{                                                                             \
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                            \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  bool shouldDelete = false;                                                  \
  nsISupports *base = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this);    \
  nsrefcnt count = mRefCnt.decr(base, &shouldDelete);                         \
  NS_LOG_RELEASE(this, count, #_class);                                       \
  if (count == 0) {                                                           \
      mRefCnt.incr(base);                                                     \
      _last;                                                                  \
      mRefCnt.decr(base);                                                     \
      if (shouldDelete) {                                                     \
          mRefCnt.stabilizeForDeletion();                                     \
          DeleteCycleCollectable();                                           \
      }                                                                       \
  }                                                                           \
  return count;                                                               \
}                                                                             \
NS_IMETHODIMP_(void) _class::DeleteCycleCollectable(void)                     \
{                                                                             \
  delete this;                                                                \
}

///////////////////////////////////////////////////////////////////////////////

/**
 * There are two ways of implementing QueryInterface, and we use both:
 *
 * Table-driven QueryInterface uses a static table of IID->offset mappings
 * and a shared helper function. Using it tends to reduce codesize and improve
 * runtime performance (due to processor cache hits).
 *
 * Macro-driven QueryInterface generates a QueryInterface function directly
 * using common macros. This is necessary if special QueryInterface features
 * are being used (such as tearoffs and conditional interfaces).
 *
 * These methods can be combined into a table-driven function call followed
 * by custom code for tearoffs and conditionals.
 */

struct QITableEntry
{
  const nsIID *iid;     // null indicates end of the QITableEntry array
  int32_t   offset;
};

NS_COM_GLUE nsresult NS_FASTCALL
NS_TableDrivenQI(void* aThis, REFNSIID aIID,
                 void **aInstancePtr, const QITableEntry* entries);

/**
 * Implement table-driven queryinterface
 */

#define NS_INTERFACE_TABLE_HEAD(_class)                                       \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                             \
  NS_ASSERTION(aInstancePtr,                                                  \
               "QueryInterface requires a non-NULL destination!");            \
  nsresult rv = NS_ERROR_FAILURE;

#define NS_INTERFACE_TABLE_BEGIN                                              \
  static const QITableEntry table[] = {

#define NS_INTERFACE_TABLE_ENTRY(_class, _interface)                          \
  { &NS_GET_IID(_interface),                                                  \
    int32_t(reinterpret_cast<char*>(                                          \
                        static_cast<_interface*>((_class*) 0x1000)) -         \
               reinterpret_cast<char*>((_class*) 0x1000))                     \
  },

#define NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, _interface, _implClass)    \
  { &NS_GET_IID(_interface),                                                  \
    int32_t(reinterpret_cast<char*>(                                          \
                        static_cast<_interface*>(                             \
                                       static_cast<_implClass*>(              \
                                                      (_class*) 0x1000))) -   \
               reinterpret_cast<char*>((_class*) 0x1000))                     \
  },

/*
 * XXX: we want to use mozilla::ArrayLength (or equivalent,
 * MOZ_ARRAY_LENGTH) in this condition, but some versions of GCC don't
 * see that the static_assert condition is actually constant in those
 * cases, even with constexpr support (?).
 */
#define NS_INTERFACE_TABLE_END_WITH_PTR(_ptr)                                 \
  { nullptr, 0 } };                                                           \
  static_assert((sizeof(table)/sizeof(table[0])) > 1, "need at least 1 interface"); \
  rv = NS_TableDrivenQI(static_cast<void*>(_ptr),                             \
                        aIID, aInstancePtr, table);

#define NS_INTERFACE_TABLE_END                                                \
  NS_INTERFACE_TABLE_END_WITH_PTR(this)

#define NS_INTERFACE_TABLE_TAIL                                               \
  return rv;                                                                  \
}

#define NS_INTERFACE_TABLE_TAIL_INHERITING(_baseclass)                        \
  if (NS_SUCCEEDED(rv))                                                       \
    return rv;                                                                \
  return _baseclass::QueryInterface(aIID, aInstancePtr);                      \
}

#define NS_INTERFACE_TABLE_TAIL_USING_AGGREGATOR(_aggregator)                 \
  if (NS_SUCCEEDED(rv))                                                       \
    return rv;                                                                \
  NS_ASSERTION(_aggregator, "null aggregator");                               \
  return _aggregator->QueryInterface(aIID, aInstancePtr)                      \
}

/**
 * This implements query interface with two assumptions: First, the
 * class in question implements nsISupports and its own interface and
 * nothing else. Second, the implementation of the class's primary
 * inheritance chain leads to its own interface.
 *
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_QUERY_HEAD(_class)                                            \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                             \
  NS_ASSERTION(aInstancePtr,                                                  \
               "QueryInterface requires a non-NULL destination!");            \
  nsISupports* foundInterface;

#define NS_IMPL_QUERY_BODY(_interface)                                        \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                                  \
    foundInterface = static_cast<_interface*>(this);                          \
  else

#define NS_IMPL_QUERY_BODY_CONDITIONAL(_interface, condition)                 \
  if ( (condition) && aIID.Equals(NS_GET_IID(_interface)))                    \
    foundInterface = static_cast<_interface*>(this);                          \
  else

#define NS_IMPL_QUERY_BODY_AMBIGUOUS(_interface, _implClass)                  \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                                  \
    foundInterface = static_cast<_interface*>(                                \
                                    static_cast<_implClass*>(this));          \
  else

#define NS_IMPL_QUERY_BODY_AGGREGATED(_interface, _aggregate)                 \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                                  \
    foundInterface = static_cast<_interface*>(_aggregate);                    \
  else

#define NS_IMPL_QUERY_TAIL_GUTS                                               \
    foundInterface = 0;                                                       \
  nsresult status;                                                            \
  if ( !foundInterface )                                                      \
    {                                                                         \
      /* nsISupports should be handled by this point. If not, fail. */        \
      MOZ_ASSERT(!aIID.Equals(NS_GET_IID(nsISupports)));                      \
      status = NS_NOINTERFACE;                                                \
    }                                                                         \
  else                                                                        \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = NS_OK;                                                         \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_IMPL_QUERY_TAIL_INHERITING(_baseclass)                             \
    foundInterface = 0;                                                       \
  nsresult status;                                                            \
  if ( !foundInterface )                                                      \
    status = _baseclass::QueryInterface(aIID, (void**)&foundInterface);       \
  else                                                                        \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = NS_OK;                                                         \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_IMPL_QUERY_TAIL_USING_AGGREGATOR(_aggregator)                      \
    foundInterface = 0;                                                       \
  nsresult status;                                                            \
  if ( !foundInterface ) {                                                    \
    NS_ASSERTION(_aggregator, "null aggregator");                             \
    status = _aggregator->QueryInterface(aIID, (void**)&foundInterface);      \
  } else                                                                      \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = NS_OK;                                                         \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_IMPL_QUERY_TAIL(_supports_interface)                               \
  NS_IMPL_QUERY_BODY_AMBIGUOUS(nsISupports, _supports_interface)              \
  NS_IMPL_QUERY_TAIL_GUTS


  /*
    This is the new scheme.  Using this notation now will allow us to switch to
    a table driven mechanism when it's ready.  Note the difference between this
    and the (currently) underlying NS_IMPL_QUERY_INTERFACE mechanism.  You must
    explicitly mention |nsISupports| when using the interface maps.
  */
#define NS_INTERFACE_MAP_BEGIN(_implClass)      NS_IMPL_QUERY_HEAD(_implClass)
#define NS_INTERFACE_MAP_ENTRY(_interface)      NS_IMPL_QUERY_BODY(_interface)
#define NS_INTERFACE_MAP_ENTRY_CONDITIONAL(_interface, condition)             \
  NS_IMPL_QUERY_BODY_CONDITIONAL(_interface, condition)
#define NS_INTERFACE_MAP_ENTRY_AGGREGATED(_interface,_aggregate)              \
  NS_IMPL_QUERY_BODY_AGGREGATED(_interface,_aggregate)

#define NS_INTERFACE_MAP_END                    NS_IMPL_QUERY_TAIL_GUTS
#define NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(_interface, _implClass)              \
  NS_IMPL_QUERY_BODY_AMBIGUOUS(_interface, _implClass)
#define NS_INTERFACE_MAP_END_INHERITING(_baseClass)                           \
  NS_IMPL_QUERY_TAIL_INHERITING(_baseClass)
#define NS_INTERFACE_MAP_END_AGGREGATED(_aggregator)                          \
  NS_IMPL_QUERY_TAIL_USING_AGGREGATOR(_aggregator)

#define NS_INTERFACE_TABLE0(_class)                                           \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, nsISupports)                             \
  NS_INTERFACE_TABLE_END

#define NS_INTERFACE_TABLE(aClass, ...)                                       \
  MOZ_STATIC_ASSERT_VALID_ARG_COUNT(__VA_ARGS__);                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    MOZ_FOR_EACH(NS_INTERFACE_TABLE_ENTRY, (aClass,), (__VA_ARGS__))          \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(aClass, nsISupports,                   \
                                       MOZ_ARG_1(__VA_ARGS__))                \
  NS_INTERFACE_TABLE_END

#define NS_IMPL_QUERY_INTERFACE0(_class)                                      \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE0(_class)                                                 \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE(aClass, ...)                                  \
  NS_INTERFACE_TABLE_HEAD(aClass)                                             \
  NS_INTERFACE_TABLE(aClass, __VA_ARGS__)                                     \
  NS_INTERFACE_TABLE_TAIL

/**
 * Declare that you're going to inherit from something that already
 * implements nsISupports, but also implements an additional interface, thus
 * causing an ambiguity. In this case you don't need another mRefCnt, you
 * just need to forward the definitions to the appropriate superclass. E.g.
 *
 * class Bar : public Foo, public nsIBar {  // both provide nsISupports
 * public:
 *   NS_DECL_ISUPPORTS_INHERITED
 *   ...other nsIBar and Bar methods...
 * };
 */
#define NS_DECL_ISUPPORTS_INHERITED                                           \
public:                                                                       \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);                          \
  NS_IMETHOD_(MozExternalRefCountType) Release(void);                         \

/**
 * These macros can be used in conjunction with NS_DECL_ISUPPORTS_INHERITED
 * to implement the nsISupports methods, forwarding the invocations to a
 * superclass that already implements nsISupports.
 *
 * Note that I didn't make these inlined because they're virtual methods.
 */

#define NS_IMPL_ADDREF_INHERITED(Class, Super)                                \
NS_IMETHODIMP_(MozExternalRefCountType) Class::AddRef(void)                   \
{                                                                             \
  nsrefcnt r = Super::AddRef();                                               \
  NS_LOG_ADDREF(this, r, #Class, sizeof(*this));                              \
  return r;                                                                   \
}

#define NS_IMPL_RELEASE_INHERITED(Class, Super)                               \
NS_IMETHODIMP_(MozExternalRefCountType) Class::Release(void)                  \
{                                                                             \
  nsrefcnt r = Super::Release();                                              \
  NS_LOG_RELEASE(this, r, #Class);                                            \
  return r;                                                                   \
}

/**
 * As above but not logging the addref/release; needed if the base
 * class might be aggregated.
 */
#define NS_IMPL_NONLOGGING_ADDREF_INHERITED(Class, Super)                     \
NS_IMETHODIMP_(MozExternalRefCountType) Class::AddRef(void)                   \
{                                                                             \
  return Super::AddRef();                                                     \
}

#define NS_IMPL_NONLOGGING_RELEASE_INHERITED(Class, Super)                    \
NS_IMETHODIMP_(MozExternalRefCountType) Class::Release(void)                  \
{                                                                             \
  return Super::Release();                                                    \
}

#define NS_INTERFACE_TABLE_INHERITED0(Class) /* Nothing to do here */

#define NS_INTERFACE_TABLE_INHERITED(aClass, ...)                             \
  MOZ_STATIC_ASSERT_VALID_ARG_COUNT(__VA_ARGS__);                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    MOZ_FOR_EACH(NS_INTERFACE_TABLE_ENTRY, (aClass,), (__VA_ARGS__))          \
  NS_INTERFACE_TABLE_END

#define NS_IMPL_QUERY_INTERFACE_INHERITED0(aClass, aSuper)                    \
  NS_INTERFACE_TABLE_HEAD(aClass)                                             \
  NS_INTERFACE_TABLE_INHERITED0(aClass)                                       \
  NS_INTERFACE_TABLE_TAIL_INHERITING(aSuper)

#define NS_IMPL_QUERY_INTERFACE_INHERITED(aClass, aSuper, ...)                \
  NS_INTERFACE_TABLE_HEAD(aClass)                                             \
  NS_INTERFACE_TABLE_INHERITED(aClass, __VA_ARGS__)                           \
  NS_INTERFACE_TABLE_TAIL_INHERITING(aSuper)

/**
 * Convenience macros for implementing all nsISupports methods for
 * a simple class.
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_ISUPPORTS0(_class)                                            \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE0(_class)

#define NS_IMPL_ISUPPORTS(aClass, ...)                                        \
  NS_IMPL_ADDREF(aClass)                                                      \
  NS_IMPL_RELEASE(aClass)                                                     \
  NS_IMPL_QUERY_INTERFACE(aClass, __VA_ARGS__)

#define NS_IMPL_ISUPPORTS_INHERITED0(aClass, aSuper)                          \
    NS_IMPL_QUERY_INTERFACE_INHERITED0(aClass, aSuper)                        \
    NS_IMPL_ADDREF_INHERITED(aClass, aSuper)                                  \
    NS_IMPL_RELEASE_INHERITED(aClass, aSuper)                                 \

#define NS_IMPL_ISUPPORTS_INHERITED(aClass, aSuper, ...)                      \
  NS_IMPL_QUERY_INTERFACE_INHERITED(aClass, aSuper, __VA_ARGS__)              \
  NS_IMPL_ADDREF_INHERITED(aClass, aSuper)                                    \
  NS_IMPL_RELEASE_INHERITED(aClass, aSuper)

/*
 * Macro to glue together a QI that starts with an interface table
 * and segues into an interface map (e.g. it uses singleton classinfo
 * or tearoffs).
 */
#define NS_INTERFACE_TABLE_TO_MAP_SEGUE \
  if (rv == NS_OK) return rv; \
  nsISupports* foundInterface;


///////////////////////////////////////////////////////////////////////////////
/**
 *
 * Threadsafe implementations of the ISupports convenience macros.
 *
 * @note  These are not available when linking against the standalone glue,
 *        because the implementation requires PR_ symbols.
 */
#define NS_INTERFACE_MAP_END_THREADSAFE NS_IMPL_QUERY_TAIL_GUTS

/**
 * Macro to generate nsIClassInfo methods for classes which do not have
 * corresponding nsIFactory implementations.
 */
#define NS_IMPL_THREADSAFE_CI(_class)                                         \
NS_IMETHODIMP                                                                 \
_class::GetInterfaces(uint32_t* _count, nsIID*** _array)                      \
{                                                                             \
  return NS_CI_INTERFACE_GETTER_NAME(_class)(_count, _array);                 \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetHelperForLanguage(uint32_t _language, nsISupports** _retval)       \
{                                                                             \
  *_retval = nullptr;                                                         \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetContractID(char** _contractID)                                     \
{                                                                             \
  *_contractID = nullptr;                                                     \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassDescription(char** _classDescription)                         \
{                                                                             \
  *_classDescription = nullptr;                                               \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassID(nsCID** _classID)                                          \
{                                                                             \
  *_classID = nullptr;                                                        \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetImplementationLanguage(uint32_t* _language)                        \
{                                                                             \
  *_language = nsIProgrammingLanguage::CPLUSPLUS;                             \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetFlags(uint32_t* _flags)                                            \
{                                                                             \
  *_flags = nsIClassInfo::THREADSAFE;                                         \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassIDNoAlloc(nsCID* _classIDNoAlloc)                             \
{                                                                             \
  return NS_ERROR_NOT_AVAILABLE;                                              \
}

#endif
