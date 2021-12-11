/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadAnnotation.h"

#include <stddef.h>

#include "mozilla/Assertions.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtr.h"

#include "prthread.h"
#include "nsDebug.h"
#include "nsExceptionHandler.h"
#include "nsString.h"
#include "nsTArray.h"

using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::UniquePtr;

namespace CrashReporter {

namespace {

#ifdef XP_MACOSX
/*
 * On the Mac, exception handler callbacks are invoked in a context where all
 * other threads are paused. As a result, attempting to acquire a mutex is
 * problematic because 1) the mutex may be held by another thread which is
 * now suspended and 2) acquiring an unheld mutex can trigger memory allocation
 * which generally requires allocator locks. This class is a wrapper around a
 * StaticMutex, providing an IsLocked() method which only makes sense to use
 * in the Mac exception handling context when other threads are paused.
 */
class MacCrashReporterLock {
 public:
  void Lock() {
    sInnerMutex.Lock();
    sIsLocked = true;
  }
  void Unlock() {
    sIsLocked = false;
    sInnerMutex.Unlock();
  }
  /*
   * Returns true if the lock is held at the time the method is called.
   * The return value is out-of-date by the time this method returns unless
   * we have a guarantee that other threads are not running such as in Mac
   * breadkpad exception handler context.
   */
  bool IsLocked() { return sIsLocked; }
  void AssertCurrentThreadOwns() { sInnerMutex.AssertCurrentThreadOwns(); }

 private:
  static StaticMutex sInnerMutex;
  static bool sIsLocked;
};
StaticMutex MacCrashReporterLock::sInnerMutex;
bool MacCrashReporterLock::sIsLocked;

// Use MacCrashReporterLock for locking
typedef mozilla::detail::BaseAutoLock<MacCrashReporterLock&>
    CrashReporterAutoLock;
typedef MacCrashReporterLock CrashReporterLockType;
#else  /* !XP_MACOSX */
// Use StaticMutex for locking
typedef StaticMutexAutoLock CrashReporterAutoLock;
typedef StaticMutex CrashReporterLockType;
#endif /* XP_MACOSX */

// Protects access to sInitialized and sThreadAnnotations.
static CrashReporterLockType sMutex;

class ThreadAnnotationSpan {
 public:
  ThreadAnnotationSpan(uint32_t aBegin, uint32_t aEnd)
      : mBegin(aBegin), mEnd(aEnd) {
    MOZ_ASSERT(mBegin < mEnd);
  }

  ~ThreadAnnotationSpan();

  class Comparator {
   public:
    bool Equals(const ThreadAnnotationSpan* const& a,
                const ThreadAnnotationSpan* const& b) const {
      return a->mBegin == b->mBegin;
    }

    bool LessThan(const ThreadAnnotationSpan* const& a,
                  const ThreadAnnotationSpan* const& b) const {
      return a->mBegin < b->mBegin;
    }
  };

 private:
  // ~ThreadAnnotationSpan() does nontrivial thing. Make sure we don't
  // instantiate accidentally.
  ThreadAnnotationSpan(const ThreadAnnotationSpan& aOther) = delete;
  ThreadAnnotationSpan& operator=(const ThreadAnnotationSpan& aOther) = delete;

  friend class ThreadAnnotationData;
  friend class Comparator;

  uint32_t mBegin;
  uint32_t mEnd;
};

// This class keeps the flat version of thread annotations for each thread.
// When a thread calls CrashReporter::SetCurrentThreadName(), it adds
// information about the calling thread (thread id and name) to this class.
// When crash happens, the crash reporter gets flat representation and add to
// the crash annotation file.
class ThreadAnnotationData {
 public:
  ThreadAnnotationData() = default;

  ~ThreadAnnotationData() = default;

  // Adds <pre> tid:"thread name",</pre> annotation to the current annotations.
  // Returns an instance of ThreadAnnotationSpan for cleanup on thread
  // termination.
  ThreadAnnotationSpan* AddThreadAnnotation(ThreadId aTid,
                                            const char* aThreadName) {
    if (!aTid || !aThreadName) {
      return nullptr;
    }

    uint32_t oldLength = mData.Length();
    mData.AppendPrintf("%u:\"%s\",", aTid, aThreadName);
    uint32_t newLength = mData.Length();

    ThreadAnnotationSpan* rv = new ThreadAnnotationSpan(oldLength, newLength);
    mDataSpans.AppendElement(rv);
    return rv;
  }

  // Called on thread termination. Removes the thread annotation, represented as
  // ThreadAnnotationSpan, from the flat representation.
  void EraseThreadAnnotation(const ThreadAnnotationSpan& aThreadInfo) {
    uint32_t begin = aThreadInfo.mBegin;
    uint32_t end = aThreadInfo.mEnd;

    if (!(begin < end && end <= mData.Length())) {
      return;
    }

    uint32_t cutLength = end - begin;
    mData.Cut(begin, cutLength);

    // Adjust the ThreadAnnotationSpan affected by data shifting.
    size_t index = mDataSpans.BinaryIndexOf(&aThreadInfo,
                                            ThreadAnnotationSpan::Comparator());
    for (size_t i = index + 1; i < mDataSpans.Length(); i++) {
      ThreadAnnotationSpan* elem = mDataSpans[i];

      MOZ_ASSERT(elem->mBegin >= cutLength);
      MOZ_ASSERT(elem->mEnd > cutLength);

      elem->mBegin -= cutLength;
      elem->mEnd -= cutLength;
    }

    // No loner tracking aThreadInfo.
    mDataSpans.RemoveElementAt(index);
  }

  // Gets the flat representation of thread annotations.
  void GetData(const std::function<void(const char*)>& aCallback) {
    aCallback(mData.BeginReading());
  }

 private:
  // The flat representation of thread annotations.
  nsCString mData;

  // This array tracks the created ThreadAnnotationSpan instances so that we
  // can make adjustments accordingly when we cut substrings from mData on
  // thread exit.
  nsTArray<ThreadAnnotationSpan*> mDataSpans;
};

static bool sInitialized = false;
static UniquePtr<ThreadAnnotationData> sThreadAnnotations;

static unsigned sTLSThreadInfoKey = (unsigned)-1;
void ThreadLocalDestructor(void* aUserData) {
  MOZ_ASSERT(aUserData);

  CrashReporterAutoLock lock(sMutex);

  ThreadAnnotationSpan* aThreadInfo =
      static_cast<ThreadAnnotationSpan*>(aUserData);
  delete aThreadInfo;
}

// This is called on thread termination.
ThreadAnnotationSpan::~ThreadAnnotationSpan() {
  // Note that we can't lock the mutex here because this function may be called
  // from SetCurrentThreadName().
  sMutex.AssertCurrentThreadOwns();

  if (sThreadAnnotations) {
    sThreadAnnotations->EraseThreadAnnotation(*this);
  }
}

}  // Anonymous namespace.

void InitThreadAnnotation() {
  CrashReporterAutoLock lock(sMutex);

  if (sInitialized) {
    return;
  }

  PRStatus status =
      PR_NewThreadPrivateIndex(&sTLSThreadInfoKey, &ThreadLocalDestructor);
  if (status == PR_FAILURE) {
    return;
  }

  sInitialized = true;

  sThreadAnnotations = mozilla::MakeUnique<ThreadAnnotationData>();
}

void SetCurrentThreadName(const char* aName) {
  if (PR_GetThreadPrivate(sTLSThreadInfoKey)) {
    // Explicitly set TLS value to null (and call the dtor function ) before
    // acquiring sMutex to avoid reentrant deadlock.
    PR_SetThreadPrivate(sTLSThreadInfoKey, nullptr);
  }

  CrashReporterAutoLock lock(sMutex);

  if (!sInitialized) {
    return;
  }

  ThreadAnnotationSpan* threadInfo =
      sThreadAnnotations->AddThreadAnnotation(CurrentThreadId(), aName);
  // This may destroy the old insatnce.
  PR_SetThreadPrivate(sTLSThreadInfoKey, threadInfo);
}

void GetFlatThreadAnnotation(const std::function<void(const char*)>& aCallback,
                             bool aIsHandlingException) {
  bool lockNeeded = true;

#ifdef XP_MACOSX
  if (aIsHandlingException) {
    // Don't acquire the lock on Mac because we are
    // executing in exception context where all other
    // threads are paused. If the lock is held, skip
    // thread annotations to avoid deadlock caused by
    // waiting for a suspended thread. If the lock
    // isn't held, acquiring it serves no purpose and
    // can trigger memory allocations.
    if (sMutex.IsLocked()) {
      aCallback("");
      return;
    }
    lockNeeded = false;
  }
#endif

  if (lockNeeded) {
    sMutex.Lock();
  }

  if (sThreadAnnotations) {
    sThreadAnnotations->GetData(aCallback);
  } else {
    // Maybe already shutdown: call aCallback with empty annotation data.
    aCallback("");
  }

  if (lockNeeded) {
    sMutex.Unlock();
  }
}

void ShutdownThreadAnnotation() {
  CrashReporterAutoLock lock(sMutex);

  sInitialized = false;
  sThreadAnnotations.reset();
}

}  // namespace CrashReporter
