/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_THREADS_EVENTTARGETCAPABILITY_H_
#define XPCOM_THREADS_EVENTTARGETCAPABILITY_H_

#include "mozilla/ThreadSafety.h"
#include "nsIEventTarget.h"

namespace mozilla {

// A helper to ensure that data access and function usage only take place on a
// specific nsIEventTarget.
//
// This class works with Clang's thread safety analysis so that static analysis
// can ensure AssertOnCurrentThread is called before using guarded data and
// functions.
//
// This means using the class is similar to calling
// `MOZ_ASSERT(mTarget->IsOnCurrentThread())`
// prior to accessing things we expect to be on `mTarget`. However, using this
// helper has the added benefit that static analysis will warn you if you
// fail to assert prior to usage.
//
// The following is a basic example of a class using this to ensure
// a data member is only accessed on a specific target.
//
// class SomeMediaHandlerThing {
//  public:
//   SomeMediaHandlerThing(nsIEventTarget* aTarget) : mTargetCapability(aTarget)
//   {}
//
//   void UpdateMediaState() {
//     mTargetCapability.Dispatch(
//         NS_NewRunnableFunction("UpdateMediaState", [this] {
//           mTargetCapability.AssertOnCurrentThread();
//           IncreaseMediaCount();
//         }));
//   }
//
//  private:
//   void IncreaseMediaCount() REQUIRES(mTargetCapability) { mMediaCount += 1; }
//
//   uint32_t mMediaCount GUARDED_BY(mTargetCapability) = 0;
//   EventTargetCapability<nsIEventTarget> mTargetCapability;
// };

template <typename T>
class CAPABILITY EventTargetCapability final {
  static_assert(std::is_base_of_v<nsIEventTarget, T>,
                "T must derive from nsIEventTarget");

 public:
  explicit EventTargetCapability(T* aTarget) : mTarget(aTarget) {
    MOZ_ASSERT(mTarget, "mTarget should be non-null");
  }
  ~EventTargetCapability() = default;

  EventTargetCapability(const EventTargetCapability&) = default;
  EventTargetCapability(EventTargetCapability&&) = default;
  EventTargetCapability& operator=(const EventTargetCapability&) = default;
  EventTargetCapability& operator=(EventTargetCapability&&) = default;

  void AssertOnCurrentThread() const ASSERT_CAPABILITY(this) {
    MOZ_ASSERT(mTarget->IsOnCurrentThread());
  }

  // Allow users to get the event target, so classes don't have to store the
  // target as a separate member to use it.
  T* GetEventTarget() { return mTarget; }

  // Helper to simplify dispatching to mTarget.
  nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                    uint32_t aFlags = NS_DISPATCH_NORMAL) const {
    return mTarget->Dispatch(std::move(aRunnable), aFlags);
  }

 private:
  RefPtr<T> mTarget;
};

}  // namespace mozilla

#endif  // XPCOM_THREADS_EVENTTARGETCAPABILITY_H_
