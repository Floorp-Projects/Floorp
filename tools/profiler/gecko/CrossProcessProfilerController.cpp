/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessProfilerController.h"

#include "mozilla/Move.h"
#include "mozilla/ProfilerTypes.h"
#include "nsIProfiler.h"
#include "nsIProfileSaveEvent.h"
#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsProfiler.h"
#include "ProfilerControllingProcess.h"

namespace mozilla {

static const char* sObserverTopics[] = {
  "profiler-started",
  "profiler-stopped",
  "profiler-paused",
  "profiler-resumed",
  "profiler-subprocess-gather",
  "profiler-subprocess",
};

// ProfilerObserver is a refcounted class that gets registered with the
// observer service and just forwards Observe() calls to mController.
// This indirection makes the CrossProcessProfilerController API nicer because
// it doesn't require a separate Init() method to register with the observer
// service, and because not being refcounted allows CPPC to be managed with a
// UniquePtr.
// The life time of ProfilerObserver is bounded by the life time of CPPC: CPPC
// unregisters the ProfilerObserver from the observer service in its
// destructor, and then it drops its reference to the ProfilerObserver, which
// destroys the ProfilerObserver.
class ProfilerObserver final : public nsIObserver
{
public:
  explicit ProfilerObserver(CrossProcessProfilerController& aController)
    : mController(aController)
  {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override
  {
    mController.Observe(aSubject, aTopic);
    return NS_OK;
  }

private:
  ~ProfilerObserver() {}

  CrossProcessProfilerController& mController;
};

NS_IMPL_ISUPPORTS(ProfilerObserver, nsIObserver)

CrossProcessProfilerController::CrossProcessProfilerController(
  ProfilerControllingProcess* aProcess)
  : mProcess(aProcess)
  , mObserver(new ProfilerObserver(*this))
{
  if (profiler_is_active()) {
    // If the profiler is already running in this process, start it in the
    // child process immediately.
    nsCOMPtr<nsIProfilerStartParams> currentProfilerParams;
    nsCOMPtr<nsIProfiler> profiler(do_GetService("@mozilla.org/tools/profiler;1"));
    DebugOnly<nsresult> rv = profiler->GetStartParams(getter_AddRefs(currentProfilerParams));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    StartProfiler(currentProfilerParams);
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    size_t length = ArrayLength(sObserverTopics);
    for (size_t i = 0; i < length; ++i) {
      obs->AddObserver(mObserver, sObserverTopics[i], false);
    }
  }
}

CrossProcessProfilerController::~CrossProcessProfilerController()
{
  if (!mProfile.IsEmpty()) {
    nsProfiler::GetOrCreate()->OOPExitProfile(mProfile);
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    size_t length = ArrayLength(sObserverTopics);
    for (size_t i = 0; i < length; ++i) {
      obs->RemoveObserver(mObserver, sObserverTopics[i]);
    }
  }
}

void
CrossProcessProfilerController::StartProfiler(nsIProfilerStartParams* aParams)
{
  if (NS_WARN_IF(!aParams)) {
    return;
  }

  ProfilerInitParams ipcParams;

  ipcParams.enabled() = true;
  aParams->GetEntries(&ipcParams.entries());
  aParams->GetInterval(&ipcParams.interval());
  ipcParams.features() = aParams->GetFeatures();
  ipcParams.threadFilters() = aParams->GetThreadFilterNames();

  mProcess->SendStartProfiler(ipcParams);
}

void
CrossProcessProfilerController::Observe(nsISupports* aSubject,
                                        const char* aTopic)
{
  if (!strcmp(aTopic, "profiler-subprocess-gather")) {
    // profiler-subprocess-gather is the request to capture the profile. We
    // need to tell the other process that we're interested in its profile,
    // and we tell the gatherer that we've forwarded the request, so that it
    // can keep track of the number of pending profiles.
    nsProfiler::GetOrCreate()->WillGatherOOPProfile();
    mProcess->SendGatherProfile();
  }
  else if (!strcmp(aTopic, "profiler-subprocess")) {
    // profiler-subprocess is sent once the gatherer knows that all other
    // processes have replied with their profiles. It's sent during the final
    // assembly of the parent process profile, and this is where we pass the
    // subprocess profile along.
    nsCOMPtr<nsIProfileSaveEvent> pse = do_QueryInterface(aSubject);
    if (pse) {
      if (!mProfile.IsEmpty()) {
        pse->AddSubProfile(mProfile.get());
        mProfile.Truncate();
      }
    }
  }
  // These four notifications are sent by the profiler when its corresponding
  // methods are called inside this process. These state changes just need to
  // be forwarded to the other process.
  else if (!strcmp(aTopic, "profiler-started")) {
    nsCOMPtr<nsIProfilerStartParams> params(do_QueryInterface(aSubject));
    StartProfiler(params);
  }
  else if (!strcmp(aTopic, "profiler-stopped")) {
    mProcess->SendStopProfiler();
  }
  else if (!strcmp(aTopic, "profiler-paused")) {
    mProcess->SendPauseProfiler(true);
  }
  else if (!strcmp(aTopic, "profiler-resumed")) {
    mProcess->SendPauseProfiler(false);
  }
}

// This is called in response to a SendGatherProfile request, or when the
// other process exits while the profiler is running.
void
CrossProcessProfilerController::RecvProfile(const nsCString& aProfile)
{
  // Store the profile on this object.
  mProfile = aProfile;
  // Tell the gatherer that we've received the profile from this process, but
  // don't actually give it the profile. It will request the profile once all
  // processes have replied, through the "profiler-subprocess" observer
  // notification.
  nsProfiler::GetOrCreate()->GatheredOOPProfile();
}

} // namespace mozilla
