/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProfiler_h
#define nsProfiler_h

#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ProfileJSONWriter.h"
#include "mozilla/ProportionValue.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Vector.h"
#include "nsIProfiler.h"
#include "nsITimer.h"
#include "nsServiceManagerUtils.h"
#include "ProfilerCodeAddressService.h"

class nsProfiler final : public nsIProfiler {
 public:
  nsProfiler();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILER

  nsresult Init();

  static nsProfiler* GetOrCreate() {
    nsCOMPtr<nsIProfiler> iprofiler =
        do_GetService("@mozilla.org/tools/profiler;1");
    return static_cast<nsProfiler*>(iprofiler.get());
  }

 private:
  ~nsProfiler();

  typedef mozilla::MozPromise<nsCString, nsresult, false> GatheringPromise;
  typedef mozilla::MozPromise<mozilla::SymbolTable, nsresult, true>
      SymbolTablePromise;

  RefPtr<GatheringPromise> StartGathering(double aSinceTime);
  void GatheredOOPProfile(base::ProcessId aChildPid,
                          const nsACString& aProfile);
  void FinishGathering();
  void ResetGathering();
  static void GatheringTimerCallback(nsITimer* aTimer, void* aClosure);
  void RestartGatheringTimer();

  RefPtr<SymbolTablePromise> GetSymbolTableMozPromise(
      const nsACString& aDebugPath, const nsACString& aBreakpadID);

  struct ExitProfile {
    nsCString mJSON;
    uint64_t mBufferPositionAtGatherTime;
  };

  struct PendingProfile {
    base::ProcessId childPid;

    mozilla::ProportionValue progressProportion;
    nsCString progressLocation;

    mozilla::TimeStamp lastProgressRequest;
    mozilla::TimeStamp lastProgressResponse;
    mozilla::TimeStamp lastProgressChange;

    explicit PendingProfile(base::ProcessId aChildPid) : childPid(aChildPid) {}
  };

  PendingProfile* GetPendingProfile(base::ProcessId aChildPid);
  // Returns false if the request could not be sent.
  bool SendProgressRequest(PendingProfile& aPendingProfile);

  // These fields are all related to profile gathering.
  mozilla::Vector<ExitProfile> mExitProfiles;
  mozilla::Maybe<mozilla::MozPromiseHolder<GatheringPromise>> mPromiseHolder;
  nsCOMPtr<nsIThread> mSymbolTableThread;
  mozilla::Maybe<SpliceableChunkedJSONWriter> mWriter;
  mozilla::Vector<PendingProfile> mPendingProfiles;
  bool mGathering;
  nsCOMPtr<nsITimer> mGatheringTimer;
};

#endif  // nsProfiler_h
