/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAppStartup.h"
#include "nsIFile.h"
#include "nsIStringBundle.h"
#include "nsIToolkitProfile.h"
#include "nsIWindowWatcher.h"

#include "ProfileReset.h"

#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsToolkitCompsCID.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/XREAppData.h"

#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "prtime.h"

using namespace mozilla;

extern const XREAppData* gAppData;

static const char kProfileProperties[] =
  "chrome://mozapps/locale/profile/profileSelection.properties";

/**
 * Creates a new profile with a timestamp in the name to use for profile reset.
 */
nsresult
CreateResetProfile(nsIToolkitProfileService* aProfileSvc, const nsACString& aOldProfileName, nsIToolkitProfile* *aNewProfile)
{
  MOZ_ASSERT(aProfileSvc, "NULL profile service");

  nsCOMPtr<nsIToolkitProfile> newProfile;
  // Make the new profile the old profile (or "default-") + the time in seconds since epoch for uniqueness.
  nsAutoCString newProfileName;
  if (!aOldProfileName.IsEmpty()) {
    newProfileName.Assign(aOldProfileName);
    newProfileName.Append("-");
  } else {
    newProfileName.AssignLiteral("default-");
  }
  newProfileName.Append(nsPrintfCString("%" PRId64, PR_Now() / 1000));
  nsresult rv = aProfileSvc->CreateProfile(nullptr, // choose a default dir for us
                                           newProfileName,
                                           getter_AddRefs(newProfile));
  if (NS_FAILED(rv)) return rv;

  rv = aProfileSvc->Flush();
  if (NS_FAILED(rv)) return rv;

  newProfile.swap(*aNewProfile);

  return NS_OK;
}

/**
 * Delete the profile directory being reset after a backup and delete the local profile directory.
 */
nsresult
ProfileResetCleanup(nsIToolkitProfile* aOldProfile)
{
  nsresult rv;
  nsCOMPtr<nsIFile> profileDir;
  rv = aOldProfile->GetRootDir(getter_AddRefs(profileDir));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFile> profileLocalDir;
  rv = aOldProfile->GetLocalDir(getter_AddRefs(profileLocalDir));
  if (NS_FAILED(rv)) return rv;

  // Get the friendly name for the backup directory.
  nsCOMPtr<nsIStringBundleService> sbs = mozilla::services::GetStringBundleService();
  if (!sbs) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> sb;
  Unused << sbs->CreateBundle(kProfileProperties, getter_AddRefs(sb));
  if (!sb) return NS_ERROR_FAILURE;

  NS_ConvertUTF8toUTF16 appName(gAppData->name);
  const char16_t* params[] = {appName.get(), appName.get()};

  nsAutoString resetBackupDirectoryName;

  static const char* kResetBackupDirectory = "resetBackupDirectory";
  rv = sb->FormatStringFromName(kResetBackupDirectory, params, 2,
                                resetBackupDirectoryName);
  if (NS_FAILED(rv)) return rv;

  // Get info to copy the old root profile dir to the desktop as a backup.
  nsCOMPtr<nsIFile> backupDest, containerDest, profileDest;
  rv = NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(backupDest));
  if (NS_FAILED(rv)) {
    // Fall back to the home directory if the desktop is not available.
    rv = NS_GetSpecialDirectory(NS_OS_HOME_DIR, getter_AddRefs(backupDest));
    if (NS_FAILED(rv)) return rv;
  }

  // Try to create a directory for all the backups
  backupDest->Clone(getter_AddRefs(containerDest));
  containerDest->Append(resetBackupDirectoryName);
  rv = containerDest->Create(nsIFile::DIRECTORY_TYPE, 0700);
  // It's OK if it already exists, if and only if it is a directory
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    bool containerIsDir;
    rv = containerDest->IsDirectory(&containerIsDir);
    if (NS_FAILED(rv) || !containerIsDir) {
      return rv;
    }
  } else if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the name of the profile
  nsAutoString leafName;
  rv = profileDir->GetLeafName(leafName);
  if (NS_FAILED(rv)) return rv;

  // Try to create a unique directory for the profile:
  containerDest->Clone(getter_AddRefs(profileDest));
  profileDest->Append(leafName);
  rv = profileDest->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_FAILED(rv)) return rv;

  // Get the unique profile name
  rv = profileDest->GetLeafName(leafName);
  if (NS_FAILED(rv)) return rv;

  // Delete the empty directory that CreateUnique just created.
  rv = profileDest->Remove(false);
  if (NS_FAILED(rv)) return rv;

  // Show a progress window while the cleanup happens since the disk I/O can take time.
  nsCOMPtr<nsIWindowWatcher> windowWatcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (!windowWatcher) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
  if (!appStartup) return NS_ERROR_FAILURE;

  nsCOMPtr<mozIDOMWindowProxy> progressWindow;
  rv = windowWatcher->OpenWindow(nullptr,
                                 kResetProgressURL,
                                 "_blank",
                                 "centerscreen,chrome,titlebar",
                                 nullptr,
                                 getter_AddRefs(progressWindow));
  if (NS_FAILED(rv)) return rv;

  // Create a new thread to do the bulk of profile cleanup to stay responsive.
  nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID);
  nsCOMPtr<nsIThread> cleanupThread;
  rv = tm->NewThread(0, 0, getter_AddRefs(cleanupThread));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRunnable> runnable = new ProfileResetCleanupAsyncTask(profileDir, profileLocalDir,
                                                                      containerDest, leafName);
    cleanupThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL);
    // The result callback will shut down the worker thread.

    // Wait for the cleanup thread to complete.
    SpinEventLoopUntil([&]() { return gProfileResetCleanupCompleted; });
  } else {
    gProfileResetCleanupCompleted = true;
    NS_WARNING("Cleanup thread creation failed");
    return rv;
  }
  // Close the progress window now that the cleanup thread is done.
  auto* piWindow = nsPIDOMWindowOuter::From(progressWindow);
  piWindow->Close();

  // Delete the old profile from profiles.ini. The folder was already deleted by the thread above.
  rv = aOldProfile->Remove(false);
  if (NS_FAILED(rv)) NS_WARNING("Could not remove the profile");

  return rv;
}
