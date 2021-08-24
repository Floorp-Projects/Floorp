/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerThreadRegistration.h"

#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ProfilerThreadRegistry.h"
#include "nsString.h"

namespace mozilla::profiler {

/* static */
MOZ_THREAD_LOCAL(ThreadRegistration*) ThreadRegistration::tlsThreadRegistration;

ThreadRegistration::ThreadRegistration(const char* aName, const void* aStackTop)
    : mData(aName, aStackTop) {
  auto* tls = GetTLS();
  if (MOZ_UNLIKELY(!tls)) {
    // No TLS, nothing can be done without it.
    return;
  }

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
    PROFILER_MARKER_TEXT("Nested ThreadRegistration()", OTHER_Profiling,
                         MarkerOptions{},
                         ProfilerString8View::WrapNullTerminatedString(aName));
    return;
  }

  tls->set(this);
  ThreadRegistry::Register(OnThreadRef{*this});
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
  auto* tls = GetTLS();
  if (MOZ_UNLIKELY(!tls)) {
    // No TLS, nothing can be done without it.
    return;
  }

  if (ThreadRegistration* rootRegistration = tls->get(); rootRegistration) {
    if (rootRegistration != this) {
      // `this` is not in the TLS, so it was a nested registration, there is
      // nothing to unregister yet.
      PROFILER_MARKER_TEXT(
          "Nested ~ThreadRegistration()", OTHER_Profiling, MarkerOptions{},
          ProfilerString8View::WrapNullTerminatedString(mData.Info().Name()));
      return;
    }

    ThreadRegistry::Unregister(OnThreadRef{*this});
    tls->set(nullptr);
    return;
  }

  // Already removed from the TLS!? This could happen with improperly-nested
  // register/unregister calls, and the first ThreadRegistration has already
  // been unregistered.
  // We cannot record a marker on this thread because it was already
  // unregistered. Send it to the main thread (unless this *is* already the
  // main thread, which has been unregistered); this may be useful to catch
  // mismatched register/unregister pairs in Firefox.
  if (!profiler_is_main_thread()) {
    nsAutoCString threadId("thread id: ");
    threadId.AppendInt(profiler_current_thread_id().ToNumber());
    threadId.AppendLiteral(", name: \"");
    threadId.AppendASCII(mData.Info().Name());
    threadId.AppendLiteral("\"");
    PROFILER_MARKER_TEXT(
        "~ThreadRegistration() but TLS is empty", OTHER_Profiling,
        MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
        threadId);
  }
}

/* static */
ProfilingStack* ThreadRegistration::RegisterThread(const char* aName,
                                                   const void* aStackTop) {
  auto* tls = GetTLS();
  if (MOZ_UNLIKELY(!tls)) {
    // No TLS, nothing can be done without it.
    return nullptr;
  }

  if (ThreadRegistration* rootRegistration = tls->get(); rootRegistration) {
    // Already registered, record the extra depth to ignore the matching
    // UnregisterThread.
    ++rootRegistration->mOtherRegistrations;
    // TODO: Use new name. This is currently not possible because the
    // TLS-stored RegisteredThread's ThreadInfo cannot be changed.
    // In the meantime, we record a marker that could be used in the frontend.
    PROFILER_MARKER_TEXT("Nested ThreadRegistration::RegisterThread()",
                         OTHER_Profiling, MarkerOptions{},
                         ProfilerString8View::WrapNullTerminatedString(aName));
    return &rootRegistration->mData.mProfilingStack;
  }

  // Create on heap, it self-registers with the TLS (its effective owner, so
  // we can forget the pointer after this), and with the Profiler.
  ThreadRegistration* tr = new ThreadRegistration(aName, aStackTop);
  tr->mIsOnHeap = true;
  return &tr->mData.mProfilingStack;
}

/* static */
void ThreadRegistration::UnregisterThread() {
  auto* tls = GetTLS();
  if (MOZ_UNLIKELY(!tls)) {
    // No TLS, nothing can be done without it.
    return;
  }

  if (ThreadRegistration* rootRegistration = tls->get(); rootRegistration) {
    if (rootRegistration->mOtherRegistrations != 0) {
      // This is assumed to be a matching UnregisterThread() for a nested
      // RegisterThread(). Decrease depth and we're done.
      --rootRegistration->mOtherRegistrations;
      // We don't know what name was used in the related RegisterThread().
      PROFILER_MARKER_UNTYPED("Nested ThreadRegistration::UnregisterThread()",
                              OTHER_Profiling);
      return;
    }

    if (!rootRegistration->mIsOnHeap) {
      // The root registration was not added by `RegisterThread()`, so it
      // shouldn't be deleted!
      // This could happen if there are un-paired `UnregisterThread` calls when
      // the initial registration (still alive) was done on the stack. We don't
      // know what name was used in the related RegisterThread().
      PROFILER_MARKER_UNTYPED("Excess ThreadRegistration::UnregisterThread()",
                              OTHER_Profiling, MarkerStack::Capture());
      return;
    }

    // This is the last `UnregisterThread()` that should match the first
    // `RegisterThread()` that created this ThreadRegistration on the heap.
    // Just delete this root registration, it will de-register itself from the
    // TLS (and from the Profiler).
    delete rootRegistration;
    return;
  }

  // There is no known ThreadRegistration for this thread, ignore this
  // request. We cannot record a marker on this thread because it was already
  // unregistered. Send it to the main thread (unless this *is* already the
  // main thread, which has been unregistered); this may be useful to catch
  // mismatched register/unregister pairs in Firefox.
  if (!profiler_is_main_thread()) {
    nsAutoCString threadId("thread id: ");
    threadId.AppendInt(profiler_current_thread_id().ToNumber());
    PROFILER_MARKER_TEXT(
        "ThreadRegistration::UnregisterThread() but TLS is empty",
        OTHER_Profiling,
        MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
        threadId);
  }
}

}  // namespace mozilla::profiler
