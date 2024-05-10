/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MemoryTelemetry_h
#define mozilla_MemoryTelemetry_h

#include "mozilla/TimeStamp.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

#include <functional>

class nsIEventTarget;

namespace mozilla {

namespace ipc {
enum class ResponseRejectReason;
}

/**
 * Periodically gathers memory usage metrics after cycle collection, and
 * populates telemetry histograms with their values.
 */
class MemoryTelemetry final : public nsIObserver,
                              public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static MemoryTelemetry& Get();

  nsresult GatherReports(
      const std::function<void()>& aCompletionCallback = nullptr);

  /**
   * Called to signal that we can begin collecting telemetry.
   */
  void DelayedInit();

  /**
   * Notify that the browser is active and telemetry should be recorded soon.
   */
  void Poke();

  nsresult Shutdown();

 private:
  MemoryTelemetry();

  ~MemoryTelemetry() = default;

  void Init();

  static Result<uint32_t, nsresult> GetOpenTabsCount();

  void GatherTotalMemory();
  nsresult FinishGatheringTotalMemory(Maybe<int64_t> aTotalMemory,
                                      const nsTArray<int64_t>& aChildSizes);

  nsCOMPtr<nsIEventTarget> mThreadPool;

  bool mGatheringTotalMemory = false;

  TimeStamp mLastRun{};
  TimeStamp mLastPoke{};
  nsCOMPtr<nsITimer> mTimer;

  // True if startup is finished and it's okay to start gathering telemetry.
  bool mCanRun = false;
};

}  // namespace mozilla

#endif  // defined mozilla_MemoryTelemetry_h
