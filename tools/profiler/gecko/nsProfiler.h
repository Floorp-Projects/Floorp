/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProfiler_h
#define nsProfiler_h

#include "nsIProfiler.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "nsServiceManagerUtils.h"
#include "ProfileJSONWriter.h"

class nsProfiler final : public nsIProfiler, public nsIObserver
{
public:
  nsProfiler();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIPROFILER

  nsresult Init();

  static nsProfiler* GetOrCreate()
  {
    nsCOMPtr<nsIProfiler> iprofiler =
      do_GetService("@mozilla.org/tools/profiler;1");
    return static_cast<nsProfiler*>(iprofiler.get());
  }

  void GatheredOOPProfile(const nsACString& aProfile);

private:
  ~nsProfiler();

  typedef mozilla::MozPromise<nsCString, nsresult, false> GatheringPromise;

  RefPtr<GatheringPromise> StartGathering(double aSinceTime);
  void FinishGathering();
  void ResetGathering();

  bool mLockedForPrivateBrowsing;

  // These fields are all related to profile gathering.
  nsTArray<nsCString> mExitProfiles;
  mozilla::Maybe<mozilla::MozPromiseHolder<GatheringPromise>> mPromiseHolder;
  mozilla::Maybe<SpliceableChunkedJSONWriter> mWriter;
  uint32_t mPendingProfiles;
  bool mGathering;
};

#endif // nsProfiler_h

