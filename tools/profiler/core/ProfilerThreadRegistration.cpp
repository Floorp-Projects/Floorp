/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerThreadRegistration.h"

// TODO in bug 1721939 next patch: #include "mozilla/ProfilerThreadRegistry.h"

namespace mozilla::profiler {

// Statically-initialized to 0, i.e. NOT_YET.
/* static */
ThreadRegistration::TLSInitialization ThreadRegistration::sIsTLSInitialized;

/* static */
MOZ_THREAD_LOCAL(ThreadRegistration*) ThreadRegistration::tlsThreadRegistration;

ThreadRegistration::ThreadRegistration(const char* aName, const void* aStackTop)
    : mData(aName, aStackTop) {
  if (mData.Info().IsMainThread() &&
      sIsTLSInitialized == TLSInitialization::NOT_YET) {
    sIsTLSInitialized = tlsThreadRegistration.init()
                            ? TLSInitialization::DONE
                            : TLSInitialization::FAILED;
  }

  if (auto* tls = GetTLS(); tls) {
    if (ThreadRegistration* rootRegistration = tls->get(); rootRegistration) {
      // This is a nested ThreadRegistration object, so the thread is already
      // registered in the TLS and ThreadRegistry and we don't need to register
      // again.
      return;
    }

    tls->set(this);
    // TODO in bug 1721939 next patch: ThreadRegistry::Register(*this);
  }
}

ThreadRegistration::~ThreadRegistration() {
#ifdef DEBUG
  MOZ_ASSERT(profiler_current_thread_id() == mData.mInfo.ThreadId(),
             "ThreadRegistration must be destroyed on its thread");
  MOZ_ASSERT(!mDataMutex.IsLockedOnCurrentThread(),
             "Mutex shouldn't be locked here, as it's about to be destroyed "
             "in ~ThreadRegistration()");
  MOZ_ASSERT(mDataMutex.TryLock(),
             "Mutex shouldn't be locked in any thread, as it's about to be "
             "destroyed in ~ThreadRegistration()");
  // Undo the above successful TryLock.
  mDataMutex.Unlock();
#endif  // DEBUG
  if (auto* tls = GetTLS(); tls) {
    if (tls->get() != this) {
      // This was a nested registration, nothing to unregister yet.
      return;
    }

    // TODO in bug 1721939 next patch: ThreadRegistry::Unregister(*this);
    tls->set(nullptr);
  }
}

/* static */
ProfilingStack& ThreadRegistration::RegisterThread(const char* aName,
                                                   const void* aStackTop) {
  if (ThreadRegistration* rootRegistration = GetFromTLS(); rootRegistration) {
    // Already registered, record the extra depth to ignore the matching
    // UnregisterThread.
    ++rootRegistration->mOtherRegistrations;
    return rootRegistration->mData.mProfilingStack;
  }

  // Create on heap, it self-registers with the TLS (its effective owner, so
  // we can forget the pointer after this), and with the Profiler.
  ThreadRegistration* tr = new ThreadRegistration(aName, aStackTop);
  return tr->mData.mProfilingStack;
}

/* static */
void ThreadRegistration::UnregisterThread() {
  if (ThreadRegistration* rootRegistration = GetFromTLS(); rootRegistration) {
    if (rootRegistration->mOtherRegistrations != 0) {
      // This is assumed to be a matching UnregisterThread() for a nested
      // RegisterThread(). Decrease depth and we're done.
      --rootRegistration->mOtherRegistrations;
      return;
    }
    // Just delete the root registration, it will de-register itself from the
    // TLS (and from the Profiler).
    delete rootRegistration;
  }
}

}  // namespace mozilla::profiler
