/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadAnnotation.h"

#ifdef MOZ_WIDGET_ANDROID
#include <pthread.h>
#endif
#include <stddef.h>

#include "mozilla/Assertions.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

#include "nsDebug.h"
#include "nsExceptionHandler.h"
#include "nsString.h"
#include "nsTArray.h"

using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::UniquePtr;

namespace CrashReporter {

namespace {

// Protects access to sInitialized and sThreadAnnotations.
static StaticMutex sMutex;

class ThreadAnnotationSpan {
public:
  ThreadAnnotationSpan(uint32_t aBegin, uint32_t aEnd)
    : mBegin(aBegin)
    , mEnd(aEnd)
  {
    MOZ_ASSERT(mBegin < mEnd);
  }

  ~ThreadAnnotationSpan();

  class Comparator {
  public:
     bool Equals(const ThreadAnnotationSpan* const& a,
                 const ThreadAnnotationSpan* const& b) const
     {
       return a->mBegin == b->mBegin;
     }

     bool LessThan(const ThreadAnnotationSpan* const& a,
                   const ThreadAnnotationSpan* const& b) const
     {
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
  ThreadAnnotationData()
  {}

  ~ThreadAnnotationData()
  {}

  // Adds <pre> tid:"thread name",</pre> annotation to the current annotations.
  // Returns an instance of ThreadAnnotationSpan for cleanup on thread
  // termination.
  ThreadAnnotationSpan*
  AddThreadAnnotation(ThreadId aTid, const char* aThreadName)
  {
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
  void EraseThreadAnnotation(const ThreadAnnotationSpan& aThreadInfo)
  {
    uint32_t begin = aThreadInfo.mBegin;
    uint32_t end = aThreadInfo.mEnd;

    if (!(begin < end &&
          end <= mData.Length())) {
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
  void GetData(const std::function<void(const char*)>& aCallback)
  {
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

template<typename T>
class DeleteWithLock
{
public:
  constexpr DeleteWithLock() {}

  void operator()(T* aPtr) const
  {
    static_assert(sizeof(T) > 0, "T must be complete");
    StaticMutexAutoLock lock(sMutex);

    delete aPtr;
  }
};

static bool sInitialized = false;
static UniquePtr<ThreadAnnotationData> sThreadAnnotations;

#ifdef MOZ_WIDGET_ANDROID
// Bionic libc on Android 4.0 doesn't support thread_local keyword. We need to
// use pthread_key_create() to hook thread destruction callback.
static pthread_key_t sTLSThreadInfoKey;
void ThreadExitCallback(void* aUserData)
{
  if (!aUserData) {
    return;
  }

  StaticMutexAutoLock lock(sMutex);

  ThreadAnnotationSpan* aThreadInfo =
    static_cast<ThreadAnnotationSpan*>(aUserData);
  pthread_setspecific(sTLSThreadInfoKey, nullptr);
  delete aThreadInfo;
}
#else
// The C++ thread_local keyword provides a cross-platform access to TLS.
// We use UniquePtr so that we may hook ThreadAnnotationSpan's destructor
// on thread termination and remove the thread annotation from
// sThreadAnnotations.
static thread_local
UniquePtr<ThreadAnnotationSpan, DeleteWithLock<ThreadAnnotationSpan>>
  sTLSThreadInfo;
#endif

// This is called on thread termination.
ThreadAnnotationSpan::~ThreadAnnotationSpan()
{
  // Note that we can't lock the mutex here because this function may be called
  // from SetCurrentThreadName().
  sMutex.AssertCurrentThreadOwns();

  if (sThreadAnnotations) {
    sThreadAnnotations->EraseThreadAnnotation(*this);
  }
}

} // Anonymous namespace.

void InitThreadAnnotation()
{
  StaticMutexAutoLock lock(sMutex);

  if (sInitialized) {
    return;
  }

#ifdef MOZ_WIDGET_ANDROID
  int rv = pthread_key_create(&sTLSThreadInfoKey, &ThreadExitCallback);
  if (rv) {
    return;
  }
#endif

  sInitialized = true;

  sThreadAnnotations = mozilla::MakeUnique<ThreadAnnotationData>();
}

void SetCurrentThreadName(const char* aName)
{
  StaticMutexAutoLock lock(sMutex);

  if (!sInitialized) {
    return;
  }

  // Note that the TLS variable may already has a non-null value if we set the
  // thread name again. In this case, we need to destroy the old instance.
#ifdef MOZ_WIDGET_ANDROID
  // This destroys the old instance.
  UniquePtr<ThreadAnnotationSpan> oldThreadInfo{
    static_cast<ThreadAnnotationSpan*>(pthread_getspecific(sTLSThreadInfoKey))};

  ThreadAnnotationSpan* threadInfo =
    sThreadAnnotations->AddThreadAnnotation(CurrentThreadId(),
                                            aName);
  pthread_setspecific(sTLSThreadInfoKey, threadInfo);
#else
  // This destroys the old instance. Note that we need to release sTLSThreadInfo
  // to avoid deadlock with sMutex.
  UniquePtr<ThreadAnnotationSpan> oldThreadInfo{sTLSThreadInfo.release()};

  sTLSThreadInfo.reset(
    sThreadAnnotations->AddThreadAnnotation(CurrentThreadId(), aName));
#endif
}

void GetFlatThreadAnnotation(const std::function<void(const char*)>& aCallback)
{
  StaticMutexAutoLock lock(sMutex);

  if (sThreadAnnotations) {
    sThreadAnnotations->GetData(aCallback);
  } else {
    // Maybe already shutdown: call aCallback with empty annotation data.
    aCallback("");
  }
}

void ShutdownThreadAnnotation()
{
  StaticMutexAutoLock lock(sMutex);

  sInitialized = false;
  sThreadAnnotations.reset();
}

}
