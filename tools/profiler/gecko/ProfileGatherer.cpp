/* This Source Code Form is subject to the terms of the Mozilla Public

 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfileGatherer.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIProfileSaveEvent.h"
#include "GeckoSampler.h"
#include "nsLocalFile.h"
#include "nsIFileStreams.h"

using mozilla::dom::AutoJSAPI;
using mozilla::dom::Promise;

namespace mozilla {

/**
 * When a subprocess exits before we've gathered profiles, we'll
 * store profiles for those processes until gathering starts. We'll
 * only store up to MAX_SUBPROCESS_EXIT_PROFILES. The buffer is
 * circular, so as soon as we receive another exit profile, we'll
 * bump the oldest one out of the buffer.
 */
static const uint32_t MAX_SUBPROCESS_EXIT_PROFILES = 5;

NS_IMPL_ISUPPORTS(ProfileGatherer, nsIObserver)

ProfileGatherer::ProfileGatherer(GeckoSampler* aTicker)
  : mTicker(aTicker)
  , mSinceTime(0)
  , mPendingProfiles(0)
  , mGathering(false)
{
}

void
ProfileGatherer::GatheredOOPProfile()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mGathering) {
    // If we're not actively gathering, then we don't actually
    // care that we gathered a profile here. This can happen for
    // processes that exit while profiling.
    return;
  }

  if (NS_WARN_IF(!mPromise && !mFile)) {
    // If we're not holding on to a Promise, then someone is
    // calling us erroneously.
    return;
  }

  mPendingProfiles--;

  if (mPendingProfiles == 0) {
    // We've got all of the async profiles now. Let's
    // finish off the profile and resolve the Promise.
    Finish();
  }
}

void
ProfileGatherer::WillGatherOOPProfile()
{
  mPendingProfiles++;
}

void
ProfileGatherer::Start(double aSinceTime,
                       Promise* aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mGathering) {
    // If we're already gathering, reject the promise - this isn't going
    // to end well.
    if (aPromise) {
      aPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    }
    return;
  }

  mSinceTime = aSinceTime;
  mPromise = aPromise;
  mGathering = true;
  mPendingProfiles = 0;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    DebugOnly<nsresult> rv =
      os->AddObserver(this, "profiler-subprocess", false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddObserver failed");
    rv = os->NotifyObservers(this, "profiler-subprocess-gather", nullptr);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NotifyObservers failed");
  }

  if (!mPendingProfiles) {
    Finish();
  }
}

void
ProfileGatherer::Start(double aSinceTime,
                       nsIFile* aFile)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mGathering) {
    return;
  }

  mSinceTime = aSinceTime;
  mFile = aFile;
  mGathering = true;
  mPendingProfiles = 0;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    DebugOnly<nsresult> rv =
      os->AddObserver(this, "profiler-subprocess", false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddObserver failed");
    rv = os->NotifyObservers(this, "profiler-subprocess-gather", nullptr);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NotifyObservers failed");
  }

  if (!mPendingProfiles) {
    Finish();
  }
}

void
ProfileGatherer::Start(double aSinceTime,
                       const nsACString& aFileName)
{
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  nsresult rv = file->InitWithNativePath(aFileName);
  if (NS_FAILED(rv)) {
    MOZ_CRASH();
  }
  Start(aSinceTime, file);
}

void
ProfileGatherer::Finish()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTicker) {
    // We somehow got called after we were cancelled! This shouldn't
    // be possible, but doing a belt-and-suspenders check to be sure.
    return;
  }

  UniquePtr<char[]> buf = mTicker->ToJSON(mSinceTime);

  if (mFile) {
    nsCOMPtr<nsIFileOutputStream> of =
      do_CreateInstance("@mozilla.org/network/file-output-stream;1");
    of->Init(mFile, -1, -1, 0);
    uint32_t sz;
    of->Write(buf.get(), strlen(buf.get()), &sz);
    of->Close();
    Reset();
    return;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    DebugOnly<nsresult> rv = os->RemoveObserver(this, "profiler-subprocess");
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RemoveObserver failed");
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mPromise->GlobalJSObject()))) {
    // We're really hosed if we can't get a JS context for some reason.
    Reset();
    return;
  }

  JSContext* cx = jsapi.cx();

  // Now parse the JSON so that we resolve with a JS Object.
  JS::RootedValue val(cx);
  {
    NS_ConvertUTF8toUTF16 js_string(nsDependentCString(buf.get()));
    if (!JS_ParseJSON(cx, static_cast<const char16_t*>(js_string.get()),
                      js_string.Length(), &val)) {
      if (!jsapi.HasException()) {
        mPromise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
      } else {
        JS::RootedValue exn(cx);
        DebugOnly<bool> gotException = jsapi.StealException(&exn);
        MOZ_ASSERT(gotException);

        jsapi.ClearException();
        mPromise->MaybeReject(cx, exn);
      }
    } else {
      mPromise->MaybeResolve(val);
    }
  }

  Reset();
}

void
ProfileGatherer::Reset()
{
  mSinceTime = 0;
  mPromise = nullptr;
  mFile = nullptr;
  mPendingProfiles = 0;
  mGathering = false;
}

void
ProfileGatherer::Cancel()
{
  // The GeckoSampler is going away. If we have a Promise in flight, we
  // should reject it.
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
  mPromise = nullptr;
  mFile = nullptr;

  // Clear out the GeckoSampler reference, since it's being destroyed.
  mTicker = nullptr;
}

void
ProfileGatherer::OOPExitProfile(const nsCString& aProfile)
{
  if (mExitProfiles.Length() >= MAX_SUBPROCESS_EXIT_PROFILES) {
    mExitProfiles.RemoveElementAt(0);
  }
  mExitProfiles.AppendElement(aProfile);

  // If a process exited while gathering, we need to make
  // sure we decrement the counter.
  if (mGathering) {
    GatheredOOPProfile();
  }
}

NS_IMETHODIMP
ProfileGatherer::Observe(nsISupports* aSubject,
                         const char* aTopic,
                         const char16_t *someData)
{
  if (!strcmp(aTopic, "profiler-subprocess")) {
    nsCOMPtr<nsIProfileSaveEvent> pse = do_QueryInterface(aSubject);
    if (pse) {
      for (size_t i = 0; i < mExitProfiles.Length(); ++i) {
        if (!mExitProfiles[i].IsEmpty()) {
          pse->AddSubProfile(mExitProfiles[i].get());
        }
      }
      mExitProfiles.Clear();
    }
  }
  return NS_OK;
}

} // namespace mozilla
