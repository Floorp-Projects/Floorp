/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileGatherer.h"

#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIProfileSaveEvent.h"
#include "nsLocalFile.h"
#include "nsIFileStreams.h"
#include "platform.h"

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

ProfileGatherer::ProfileGatherer()
  : mIsCancelled(false)
  , mSinceTime(0)
  , mPendingProfiles(0)
  , mGathering(false)
{
}

void
ProfileGatherer::GatheredOOPProfile(PSLockRef aLock)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

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
    Finish(aLock);
  }
}

void
ProfileGatherer::WillGatherOOPProfile()
{
  mPendingProfiles++;
}

void
ProfileGatherer::Start(PSLockRef aLock, double aSinceTime, Promise* aPromise)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mGathering) {
    // If we're already gathering, reject the promise - this isn't going
    // to end well.
    if (aPromise) {
      aPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    }
    return;
  }

  mPromise = aPromise;

  Start2(aLock, aSinceTime);
}

void
ProfileGatherer::Start(PSLockRef aLock, double aSinceTime,
                       const nsACString& aFileName)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  nsresult rv = file->InitWithNativePath(aFileName);
  if (NS_FAILED(rv)) {
    MOZ_CRASH();
  }

  if (mGathering) {
    return;
  }

  mFile = file;

  Start2(aLock, aSinceTime);
}

// This is the common tail shared by both Start() methods.
void
ProfileGatherer::Start2(PSLockRef aLock, double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mSinceTime = aSinceTime;
  mGathering = true;
  mPendingProfiles = 0;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    DebugOnly<nsresult> rv =
      os->AddObserver(this, "profiler-subprocess", false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddObserver failed");

    // This notification triggers calls back to
    // profiler_will_gather_OOP_profile(). See the comment in that function for
    // the gory details of the connection between this function and that one.
    rv = os->NotifyObservers(this, "profiler-subprocess-gather", nullptr);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NotifyObservers failed");
  }

  if (!mPendingProfiles) {
    Finish(aLock);
  }
}

void
ProfileGatherer::Finish(PSLockRef aLock)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mIsCancelled) {
    // We somehow got called after we were cancelled! This shouldn't
    // be possible, but doing a belt-and-suspenders check to be sure.
    return;
  }

  UniquePtr<char[]> buf = ToJSON(aLock, mSinceTime);

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
  // We're about to stop profiling. If we have a Promise in flight, we should
  // reject it.
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
  mPromise = nullptr;
  mFile = nullptr;

  mIsCancelled = true;
}

void
ProfileGatherer::OOPExitProfile(const nsCString& aProfile)
{
  if (mExitProfiles.Length() >= MAX_SUBPROCESS_EXIT_PROFILES) {
    mExitProfiles.RemoveElementAt(0);
  }
  mExitProfiles.AppendElement(aProfile);
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
