/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PROFILE_GATHERER_H
#define MOZ_PROFILE_GATHERER_H

#include "nsIFile.h"
#include "ProfileJSONWriter.h"
#include "mozilla/MozPromise.h"

namespace mozilla {

// This class holds the state for an async profile-gathering request.
class ProfileGatherer final : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  typedef MozPromise<nsCString, nsresult, false> ProfileGatherPromise;

  explicit ProfileGatherer();
  void GatheredOOPProfile(const nsACString& aProfile);
  RefPtr<ProfileGatherPromise> Start(double aSinceTime);
  void OOPExitProfile(const nsACString& aProfile);

private:
  ~ProfileGatherer();
  void Cancel();
  void Finish();
  void Reset();

  nsTArray<nsCString> mExitProfiles;
  Maybe<MozPromiseHolder<ProfileGatherPromise>> mPromiseHolder;
  Maybe<SpliceableChunkedJSONWriter> mWriter;
  uint32_t mPendingProfiles;
  bool mGathering;
};

} // namespace mozilla

#endif
