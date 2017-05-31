/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileGatherer.h"

#include "mozilla/Services.h"
#include "nsIObserverService.h"

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

  MOZ_RELEASE_ASSERT(mWriter.isSome(), "Should always have a writer if mGathering is true");

  if (!aProfile.IsEmpty()) {
    mWriter->Splice(PromiseFlatCString(aProfile).get());
  }

  mPendingProfiles--;

  if (mPendingProfiles == 0) {
    // We've got all of the async profiles now. Let's
    // finish off the profile and resolve the Promise.
    Finish();
  }
}

RefPtr<ProfileGatherer::ProfileGatherPromise>
ProfileGatherer::Start(double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mGathering) {
    // If we're already gathering, return a rejected promise - this isn't
    // going to end well.
    return ProfileGatherPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  mGathering = true;

  // Request profiles from the other processes. This will trigger
  // asynchronous calls to ProfileGatherer::GatheredOOPProfile as the
  // profiles arrive.
  // Do this before the call to profiler_stream_json_for_this_process because
  // that call is slow and we want to let the other processes grab their
  // profiles as soon as possible.
  nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>> profiles =
    ProfilerParent::GatherProfiles();

  mWriter.emplace();

  // Start building up the JSON result and grab the profile from this process.
  mWriter->Start(SpliceableJSONWriter::SingleLineStyle);
  if (!profiler_stream_json_for_this_process(*mWriter, aSinceTime)) {
    // The profiler is inactive. This either means that it was inactive even
    // at the time that ProfileGatherer::Start() was called, or that it was
    // stopped on a different thread since that call. Either way, we need to
    // reject the promise and stop gathering.
    return ProfileGatherPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
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

  mPromiseHolder.emplace();
  RefPtr<ProfileGatherPromise> promise = mPromiseHolder->Ensure(__func__);

  // Keep the array property "processes" and the root object in mWriter open
  // until Finish() is called. As profiles from the other processes come in,
  // they will be inserted and end up in the right spot. Finish() will close
  // the array and the root object.

  mPendingProfiles = profiles.Length();
  RefPtr<ProfileGatherer> self = this;
  for (auto profile : profiles) {
    profile->Then(AbstractThread::MainThread(), __func__,
      [self](const nsCString& aResult) {
        self->GatheredOOPProfile(aResult);
      },
      [self](PromiseRejectReason aReason) {
        self->GatheredOOPProfile(NS_LITERAL_CSTRING(""));
      });
  }
  if (!mPendingProfiles) {
    Finish();
  }

  return promise;
}

void
ProfileGatherer::Finish()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mWriter.isSome());
  MOZ_RELEASE_ASSERT(mPromiseHolder.isSome());

  // Close the "processes" array property.
  mWriter->EndArray();

  // Close the root object of the generated JSON.
  mWriter->End();

  UniquePtr<char[]> buf = mWriter->WriteFunc()->CopyData();
  nsCString result(buf.get());
  mPromiseHolder->Resolve(result, __func__);

  Reset();
}

void
ProfileGatherer::Reset()
{
  mPromiseHolder.reset();
  mPendingProfiles = 0;
  mGathering = false;
  mWriter.reset();
}

void
ProfileGatherer::Cancel()
{
  // If we have a Promise in flight, we should reject it.
  if (mPromiseHolder.isSome()) {
    mPromiseHolder->RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);
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
