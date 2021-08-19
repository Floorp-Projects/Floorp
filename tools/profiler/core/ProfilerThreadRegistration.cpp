/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerThreadRegistration.h"

#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ProfilerThreadRegistry.h"

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
      MOZ_ASSERT(
          mData.Info().ThreadId() == rootRegistration->mData.Info().ThreadId(),
          "Thread being re-registered has changed its TID");
      // TODO: Use new name. This is currently not possible because the
      // TLS-stored RegisteredThread's ThreadInfo cannot be changed.
      // In the meantime, we record a marker that could be used in the frontend.
      PROFILER_MARKER_TEXT(
          "Nested ThreadRegistration()", OTHER_Profiling,
          MarkerThreadId::MainThread(),
          ProfilerString8View::WrapNullTerminatedString(aName));
      return;
    }

    tls->set(this);
    ThreadRegistry::Register(OnThreadRef{*this});
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
    ThreadRegistration* threadRegistrationInTLS = tls->get();
    if (!threadRegistrationInTLS) {
      // Already removed from the TLS!? This could happen with improperly-nested
      // register/unregister calls, and the first ThreadRegistration has already
      // been unregistered.
      // We cannot record a marker on this thread because it was already
      // unregistered. Send it to the main thread (unless this *is* already the
      // main thread, which has been unregistered); this may be useful to catch
      // mismatched register/unregister pairs in Firefox.
      if (!profiler_is_main_thread()) {
        PROFILER_MARKER_TEXT(
            "~ThreadRegistration() but TLS is empty", OTHER_Profiling,
            MarkerThreadId::MainThread(),
            ProfilerString8View::WrapNullTerminatedString(mData.Info().Name()));
      }
      return;
    }
    if (threadRegistrationInTLS != this) {
      // This was a nested registration, nothing to unregister yet.
      PROFILER_MARKER_TEXT(
          "Nested ~ThreadRegistration()", OTHER_Profiling,
          MarkerThreadId::MainThread(),
          ProfilerString8View::WrapNullTerminatedString(mData.Info().Name()));
      return;
    }

    ThreadRegistry::Unregister(OnThreadRef{*this});
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
    PROFILER_MARKER_TEXT("Nested ThreadRegistration::RegisterThread()",
                         OTHER_Profiling, MarkerThreadId::MainThread(),
                         ProfilerString8View::WrapNullTerminatedString(aName));
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
      // We don't know what name was used in the related RegisterThread().
      PROFILER_MARKER_UNTYPED("Nested ThreadRegistration::UnregisterThread()",
                              OTHER_Profiling, MarkerThreadId::MainThread());
      return;
    }
    // Just delete the root registration, it will de-register itself from the
    // TLS (and from the Profiler).
    delete rootRegistration;
  }
}

}  // namespace mozilla::profiler
