/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_GATHERER_H
#define MOZ_PROFILE_GATHERER_H

#include "mozilla/dom/Promise.h"

class GeckoSampler;

namespace mozilla {

class ProfileGatherer final : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  ProfileGatherer(GeckoSampler* aTicker,
                  double aSinceTime,
                  mozilla::dom::Promise* aPromise);
  void WillGatherOOPProfile();
  void GatheredOOPProfile();
  void Start();

private:
  ~ProfileGatherer() {};
  void Finish();

  nsRefPtr<mozilla::dom::Promise> mPromise;
  GeckoSampler* mTicker;
  double mSinceTime;
  uint32_t mPendingProfiles;
};

} // namespace mozilla

#endif
