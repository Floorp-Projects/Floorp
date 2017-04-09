/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileGatherer.h"

#include "mozilla/Services.h"
#include "nsIObserverService.h"
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

NS_IMPL_ISUPPORTS0(ProfileGatherer)

ProfileGatherer::ProfileGatherer()
  : mPendingProfiles(0)
  , mGathering(false)
{
}

ProfileGatherer::~ProfileGatherer()
{
  Cancel();
}

void
ProfileGatherer::GatheredOOPProfile(const nsACString& aProfile)
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

  MOZ_RELEASE_ASSERT(mWriter.isSome(), "Should always have a writer if mGathering is true");

  mWriter->Splice(PromiseFlatCString(aProfile).get());

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
ProfileGatherer::Start(double aSinceTime, Promise* aPromise)
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

  Start2(aSinceTime);
}

void
ProfileGatherer::Start(double aSinceTime, const nsACString& aFileName)
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

  Start2(aSinceTime);
}

// This is the common tail shared by both Start() methods.
void
ProfileGatherer::Start2(double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mGathering = true;
  mPendingProfiles = 0;
  mWriter.emplace();

  // Send a notification to request profiles from other processes. The
  // observers of this notification will call WillGatherOOPProfile() which
  // increments mPendingProfiles.
  // Do this before the call to profiler_stream_json_for_this_process because
  // that call is slow and we want to let the other processes grab their
  // profiles as soon as possible.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    DebugOnly<nsresult> rv =
      os->NotifyObservers(this, "profiler-subprocess-gather", nullptr);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NotifyObservers failed");
  }

  // Start building up the JSON result and grab the profile from this process.
  mWriter->Start(SpliceableJSONWriter::SingleLineStyle);
  if (!profiler_stream_json_for_this_process(*mWriter, aSinceTime)) {
    // The profiler is inactive. This either means that it was inactive even
    // at the time that ProfileGatherer::Start() was called, or that it was
    // stopped on a different thread since that call. Either way, we need to
    // reject the promise and stop gathering.
    Cancel();
    return;
  }

  mWriter->StartArrayProperty("processes");

  // If we have any process exit profiles, add them immediately, and clear
  // mExitProfiles.
  for (size_t i = 0; i < mExitProfiles.Length(); ++i) {
    if (!mExitProfiles[i].IsEmpty()) {
      mWriter->Splice(mExitProfiles[i].get());
    }
  }
  mExitProfiles.Clear();

  // Keep the array property "processes" and the root object in mWriter open
  // until Finish() is called. As profiles from the other processes come in,
  // they will be inserted and end up in the right spot. Finish() will close
  // the array and the root object.

  if (!mPendingProfiles) {
    Finish();
  }
}

void
ProfileGatherer::Finish()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mWriter.isSome());

  // Close the "processes" array property.
  mWriter->EndArray();

  // Close the root object of the generated JSON.
  mWriter->End();

  UniquePtr<char[]> buf = mWriter->WriteFunc()->CopyData();

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
  mPromise = nullptr;
  mFile = nullptr;
  mPendingProfiles = 0;
  mGathering = false;
  mWriter.reset();
}

void
ProfileGatherer::Cancel()
{
  // If we have a Promise in flight, we should reject it.
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
  Reset();
}

void
ProfileGatherer::OOPExitProfile(const nsACString& aProfile)
{
  // Append the exit profile to mExitProfiles so that it can be picked up the
  // next time a profile is requested. If we're currently gathering a profile,
  // do not add this exit profile to it; chances are that we already have a
  // profile from the exiting process and we don't want another one.
  // We only keep around at most MAX_SUBPROCESS_EXIT_PROFILES exit profiles.
  if (mExitProfiles.Length() >= MAX_SUBPROCESS_EXIT_PROFILES) {
    mExitProfiles.RemoveElementAt(0);
  }
  mExitProfiles.AppendElement(aProfile);
}

} // namespace mozilla
