/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_GATHERER_H
#define MOZ_PROFILE_GATHERER_H

#include "mozilla/dom/Promise.h"

class GeckoSampler;

namespace mozilla {

class ProfileGatherer final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit ProfileGatherer(GeckoSampler* aTicker);
  void WillGatherOOPProfile();
  void GatheredOOPProfile();
  void Start(double aSinceTime, mozilla::dom::Promise* aPromise);
  void Cancel();
  void OOPExitProfile(const nsCString& aProfile);

private:
  ~ProfileGatherer() {};
  void Finish();
  void Reset();

  nsTArray<nsCString> mExitProfiles;
  RefPtr<mozilla::dom::Promise> mPromise;
  GeckoSampler* mTicker;
  double mSinceTime;
  uint32_t mPendingProfiles;
  bool mGathering;
};

} // namespace mozilla

#endif
