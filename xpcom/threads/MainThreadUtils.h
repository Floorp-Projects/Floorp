/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MainThreadUtils_h_
#define MainThreadUtils_h_

#include "mozilla/Assertions.h"
#include "mozilla/ThreadSafety.h"
#include "nscore.h"

class nsIThread;

/**
 * Get a reference to the main thread.
 *
 * @param aResult
 *   The resulting nsIThread object.
 */
extern nsresult NS_GetMainThread(nsIThread** aResult);

#ifdef MOZILLA_INTERNAL_API
bool NS_IsMainThreadTLSInitialized();
extern "C" {
bool NS_IsMainThread();
}

namespace mozilla {

/**
 * A dummy static capability for the thread safety analysis which can be
 * required by functions and members using `MOZ_REQUIRE(sMainThreadCapability)`
 * and `MOZ_GUARDED_BY(sMainThreadCapability)` and asserted using
 * `AssertIsOnMainThread()`.
 *
 * If you want a thread-safety-analysis capability for a non-main thread,
 * consider using the `EventTargetCapability` type.
 */
class MOZ_CAPABILITY("main thread") MainThreadCapability final {};
constexpr MainThreadCapability sMainThreadCapability;

#  ifdef DEBUG
void AssertIsOnMainThread() MOZ_ASSERT_CAPABILITY(sMainThreadCapability);
#  else
inline void AssertIsOnMainThread()
    MOZ_ASSERT_CAPABILITY(sMainThreadCapability) {}
#  endif

inline void ReleaseAssertIsOnMainThread()
    MOZ_ASSERT_CAPABILITY(sMainThreadCapability) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
}

}  // namespace mozilla

#endif

#endif  // MainThreadUtils_h_
