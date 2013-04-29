/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIToolkitProfileService.h"

#include "nsThreadUtils.h"

static bool gProfileResetCleanupCompleted = false;
static const char kResetProgressURL[] = "chrome://global/content/resetProfileProgress.xul";

nsresult CreateResetProfile(nsIToolkitProfileService* aProfileSvc,
                            nsIToolkitProfile* *aNewProfile);

nsresult ProfileResetCleanup(nsIToolkitProfile* aOldProfile);

class ProfileResetCleanupResultTask : public nsRunnable
{
public:
  ProfileResetCleanupResultTask()
    : mWorkerThread(do_GetCurrentThread())
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    mWorkerThread->Shutdown();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mWorkerThread;
};

class ProfileResetCleanupAsyncTask : public nsRunnable
{
public:
  ProfileResetCleanupAsyncTask(nsIFile* aProfileDir, nsIFile* aProfileLocalDir,
                               nsIFile* aTargetDir, const nsAString &aLeafName)
    : mProfileDir(aProfileDir)
    , mProfileLocalDir(aProfileLocalDir)
    , mTargetDir(aTargetDir)
    , mLeafName(aLeafName)
  { }

/**
 * Copy a root profile to a backup folder before deleting it.  Then delete the local profile dir.
 */
  NS_IMETHOD Run()
  {
    // Copy to the destination then delete the profile. A move doesn't follow links.
    nsresult rv = mProfileDir->CopyToFollowingLinks(mTargetDir, mLeafName);
    if (NS_SUCCEEDED(rv))
      rv = mProfileDir->Remove(true);
    else
      NS_WARNING("Could not backup the root profile directory");

    // If we have a separate local cache profile directory, just delete it.
    // Don't return an error if this fails so that reset can proceed if it can't be deleted.
    bool sameDir;
    nsresult rvLocal = mProfileDir->Equals(mProfileLocalDir, &sameDir);
    if (NS_SUCCEEDED(rvLocal) && !sameDir) {
      rvLocal = mProfileLocalDir->Remove(true);
      if (NS_FAILED(rvLocal)) NS_WARNING("Could not remove the old local profile directory (cache)");
    }
    gProfileResetCleanupCompleted = true;

    nsCOMPtr<nsIRunnable> resultRunnable = new ProfileResetCleanupResultTask();
    NS_DispatchToMainThread(resultRunnable);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIFile> mProfileDir;
  nsCOMPtr<nsIFile> mProfileLocalDir;
  nsCOMPtr<nsIFile> mTargetDir;
  nsAutoString mLeafName;
};
