/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_GATHERER_H
#define MOZ_PROFILE_GATHERER_H

#include "mozilla/dom/Promise.h"
#include "nsIFile.h"
#include "platform.h"

namespace mozilla {

class ProfileGatherer final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit ProfileGatherer();
  void WillGatherOOPProfile();
  void GatheredOOPProfile(PSLockRef aLock);
  void Start(PSLockRef aLock, double aSinceTime,
             mozilla::dom::Promise* aPromise);
  void Start(PSLockRef aLock, double aSinceTime, const nsACString& aFileName);
  void Cancel();
  void OOPExitProfile(const nsCString& aProfile);

private:
  ~ProfileGatherer() {};
  void Finish(PSLockRef aLock);
  void Reset();
  void Start2(PSLockRef aLock, double aSinceTime);

  nsTArray<nsCString> mExitProfiles;
  RefPtr<mozilla::dom::Promise> mPromise;
  nsCOMPtr<nsIFile> mFile;
  bool mIsCancelled;
  double mSinceTime;
  uint32_t mPendingProfiles;
  bool mGathering;
};

} // namespace mozilla

#endif
