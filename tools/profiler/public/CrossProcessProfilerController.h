/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CrossProcessProfilerController_h
#define CrossProcessProfilerController_h

#include "nsString.h"

class nsIProfilerStartParams;

namespace mozilla {

class ProfilerObserver;
class ProfilerControllingProcess;

// A class that calls methods on aProcess to coordinate the profiler in other
// processes. aProcess needs to implement a number of calls that trigger calls
// to the relevant profiler_* functions in the other process.
// RecvProfile needs to be called when the other process replies with its
// profile.
class CrossProcessProfilerController final
{
public:
  // aProcess is expected to outlast this CrossProcessProfilerController object.
  explicit CrossProcessProfilerController(ProfilerControllingProcess* aProcess);
  ~CrossProcessProfilerController();
  void RecvProfile(const nsCString& aProfile);

private:
  void StartProfiler(nsIProfilerStartParams* aParams);
  void Observe(nsISupports* aSubject, const char* aTopic);

  friend class ProfilerObserver;

  ProfilerControllingProcess* mProcess;
  RefPtr<ProfilerObserver> mObserver;
  nsCString mProfile;
};

} // namespace mozilla

#endif
