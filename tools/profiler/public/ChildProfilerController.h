/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChildProfilerController_h
#define ChildProfilerController_h

#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsStringFwd.h"

namespace mozilla {

class ProfilerChild;
class PProfilerChild;
class PProfilerParent;

// ChildProfilerController manages the setup and teardown of ProfilerChild.
// It's used on the main thread.
// It manages a background thread that ProfilerChild runs on.
class ChildProfilerController final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChildProfilerController)

  static already_AddRefed<ChildProfilerController> Create(
      mozilla::ipc::Endpoint<PProfilerChild>&& aEndpoint);

  [[nodiscard]] nsCString GrabShutdownProfileAndShutdown();
  void Shutdown();

 private:
  ChildProfilerController();
  ~ChildProfilerController();
  void Init(mozilla::ipc::Endpoint<PProfilerChild>&& aEndpoint);
  void ShutdownAndMaybeGrabShutdownProfileFirst(nsCString* aOutShutdownProfile);

  // Called on mThread:
  void SetupProfilerChild(mozilla::ipc::Endpoint<PProfilerChild>&& aEndpoint);
  void ShutdownProfilerChild(nsCString* aOutShutdownProfile);

  RefPtr<ProfilerChild> mProfilerChild;  // only accessed on mThread
  RefPtr<nsIThread> mThread;
};

}  // namespace mozilla

#endif  // ChildProfilerController_h
