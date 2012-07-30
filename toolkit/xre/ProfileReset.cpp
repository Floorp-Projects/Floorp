/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAppStartup.h"
#include "nsIDOMWindow.h"
#include "nsIFile.h"
#include "nsIStringBundle.h"
#include "nsIToolkitProfile.h"
#include "nsIWindowWatcher.h"

#include "ProfileReset.h"

#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsPrintfCString.h"
#include "nsToolkitCompsCID.h"
#include "nsXPCOMCIDInternal.h"
#include "nsXREAppData.h"

#include "mozilla/Services.h"

extern const nsXREAppData* gAppData;

static const char kProfileProperties[] =
  "chrome://mozapps/locale/profile/profileSelection.properties";

/**
 * Creates a new profile with a timestamp in the name to use for profile reset.
 */
nsresult
CreateResetProfile(nsIToolkitProfileService* aProfileSvc, nsIToolkitProfile* *aNewProfile)
{
  NS_ABORT_IF_FALSE(aProfileSvc, "NULL profile service");

  nsCOMPtr<nsIToolkitProfile> newProfile;
  // Make the new profile "default-" + the time in seconds since epoch for uniqueness.
  nsCAutoString newProfileName("default-");
  newProfileName.Append(nsPrintfCString("%lld", PR_Now() / 1000));
  nsresult rv = aProfileSvc->CreateProfile(nullptr, // choose a default dir for us
                                           nullptr, // choose a default dir for us
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
  rv = sbs->CreateBundle(kProfileProperties, getter_AddRefs(sb));
  if (!sb) return NS_ERROR_FAILURE;

  NS_ConvertUTF8toUTF16 appName(gAppData->name);
  const PRUnichar* params[] = {appName.get(), appName.get()};

  nsXPIDLString resetBackupDirectoryName;

  static const PRUnichar* kResetBackupDirectory = NS_LITERAL_STRING("resetBackupDirectory").get();
  rv = sb->FormatStringFromName(kResetBackupDirectory, params, 2,
                                getter_Copies(resetBackupDirectoryName));

  // Get info to copy the old root profile dir to the desktop as a backup.
  nsCOMPtr<nsIFile> backupDest, uniqueDest;
  rv = NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(backupDest));
  if (NS_FAILED(rv)) {
    // Fall back to the home directory if the desktop is not available.
    rv = NS_GetSpecialDirectory(NS_OS_HOME_DIR, getter_AddRefs(backupDest));
    if (NS_FAILED(rv)) return rv;
  }

  // Try to get a unique backup directory name.
  backupDest->Clone(getter_AddRefs(uniqueDest));
  uniqueDest->Append(resetBackupDirectoryName);
  rv = uniqueDest->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_FAILED(rv)) return rv;

  nsAutoString leafName;
  rv = uniqueDest->GetLeafName(leafName);
  if (NS_FAILED(rv)) return rv;
  // Delete the empty directory that CreateUnique just created.
  rv = uniqueDest->Remove(false);
  if (NS_FAILED(rv)) return rv;

  // Show a progress window while the cleanup happens since the disk I/O can take time.
  nsCOMPtr<nsIWindowWatcher> windowWatcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (!windowWatcher) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
  if (!appStartup) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> progressWindow;
  rv = windowWatcher->OpenWindow(nullptr,
                                 kResetProgressURL,
                                 "_blank",
                                 "centerscreen,chrome,titlebar",
                                 NULL,
                                 getter_AddRefs(progressWindow));
  if (NS_FAILED(rv)) return rv;

  // Create a new thread to do the bulk of profile cleanup to stay responsive.
  nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID);
  nsCOMPtr<nsIThread> cleanupThread;
  rv = tm->NewThread(0, 0, getter_AddRefs(cleanupThread));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRunnable> runnable = new ProfileResetCleanupAsyncTask(profileDir, profileLocalDir,
                                                                      backupDest, leafName);
    cleanupThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL);
    // The result callback will shut down the worker thread.

    nsIThread *thread = NS_GetCurrentThread();
    // Wait for the cleanup thread to complete.
    while(!gProfileResetCleanupCompleted) {
      NS_ProcessNextEvent(thread);
    }
  } else {
    gProfileResetCleanupCompleted = true;
    NS_WARNING("Cleanup thread creation failed");
    return rv;
  }
  // Close the progress window now that the cleanup thread is done.
  progressWindow->Close();

  // Delete the old profile from profiles.ini. The folder was already deleted above.
  rv = aOldProfile->Remove(false);
  if (NS_FAILED(rv)) NS_WARNING("Could not remove the profile");

  return rv;
}
