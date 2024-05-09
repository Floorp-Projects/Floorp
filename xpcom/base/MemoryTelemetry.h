/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MemoryTelemetry_h
#define mozilla_MemoryTelemetry_h

#include "mozilla/TimeStamp.h"
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
   * Does expensive initialization, which should happen only after startup has
   * completed, and the event loop is idle.
   */
  nsresult DelayedInit();

  nsresult Shutdown();

 private:
  MemoryTelemetry();

  ~MemoryTelemetry() = default;

  void Init();

  static Result<uint32_t, nsresult> GetOpenTabsCount();

  void GatherTotalMemory();
  nsresult FinishGatheringTotalMemory(int64_t aTotalMemory,
                                      const nsTArray<int64_t>& aChildSizes);

  nsCOMPtr<nsIEventTarget> mThreadPool;

  bool mGatheringTotalMemory = false;

  TimeStamp mLastPoll{};
};

}  // namespace mozilla

#endif  // defined mozilla_MemoryTelemetry_h
