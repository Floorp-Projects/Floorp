/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/GleanMetric.h"

#include "nsThreadUtils.h"
#include "nsWrapperCache.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla::glean {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GleanMetric, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(GleanMetric)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GleanMetric)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GleanMetric)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Maybe<SubmetricToMirrorMutex::AutoLock> GetLabeledMirrorLock() {
  static SubmetricToMirrorMutex sLabeledMirrors("sLabeledMirrors");
  auto lock = sLabeledMirrors.Lock();
  // GIFFT will work up to the end of AppShutdownTelemetry.
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown)) {
    return Nothing();
  }
  if (!*lock) {
    *lock = MakeUnique<SubmetricToLabeledMirrorMapType>();
    RefPtr<nsIRunnable> cleanupFn = NS_NewRunnableFunction(__func__, [&] {
      if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown)) {
        auto lock = sLabeledMirrors.Lock();
        *lock = nullptr;  // deletes, see UniquePtr.h
        return;
      }
      RunOnShutdown(
          [&] {
            auto lock = sLabeledMirrors.Lock();
            *lock = nullptr;  // deletes, see UniquePtr.h
          },
          ShutdownPhase::XPCOMWillShutdown);
    });
    // Both getting the main thread and dispatching to it can fail.
    // In that event we leak. Grab a pointer so we have something to NS_RELEASE
    // in that case.
    nsIRunnable* temp = cleanupFn.get();
    nsCOMPtr<nsIThread> mainThread;
    if (NS_FAILED(NS_GetMainThread(getter_AddRefs(mainThread))) ||
        NS_FAILED(mainThread->Dispatch(cleanupFn.forget(),
                                       nsIThread::DISPATCH_NORMAL))) {
      // Failed to dispatch cleanup routine.
      // First, un-leak the runnable (but only if we actually attempted
      // dispatch)
      if (!cleanupFn) {
        NS_RELEASE(temp);
      }
      // Next, cleanup immediately, and allow metrics to try again later.
      *lock = nullptr;
      return Nothing();
    }
  }

  return Some(std::move(lock));
}

}  // namespace mozilla::glean
