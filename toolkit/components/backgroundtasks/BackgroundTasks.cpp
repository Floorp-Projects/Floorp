/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BackgroundTasks.h"

#include "nsIBackgroundTasksManager.h"
#include "nsIFile.h"
#include "nsImportModule.h"
#include "nsPrintfCString.h"
#include "nsProfileLock.h"
#include "nsTSubstring.h"
#include "nsXULAppAPI.h"
#include "prenv.h"
#include "prtime.h"
#include "SpecialSystemDirectory.h"

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/LateWriteChecks.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(BackgroundTasks, nsIBackgroundTasks);

BackgroundTasks::BackgroundTasks(Maybe<nsCString> aBackgroundTask)
    : mBackgroundTask(std::move(aBackgroundTask)) {
  // Log when a background task is created.
  if (mBackgroundTask.isSome()) {
    MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Info,
            ("Created background task: %s", mBackgroundTask->get()));
  }
}

void BackgroundTasks::Init(Maybe<nsCString> aBackgroundTask) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());

  MOZ_RELEASE_ASSERT(!sSingleton,
                     "BackgroundTasks singleton already initialized");
  // The singleton will be cleaned up by `Shutdown()`.
  sSingleton = new BackgroundTasks(std::move(aBackgroundTask));
}

void BackgroundTasks::Shutdown() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());

  MOZ_LOG(sBackgroundTasksLog, LogLevel::Info, ("Shutdown"));

  if (!sSingleton) {
    return;
  }

  if (sSingleton->mProfD &&
      !EnvHasValue("MOZ_BACKGROUNDTASKS_NO_REMOVE_PROFILE")) {
    AutoSuspendLateWriteChecks suspend;

    // Log that the temporary profile is being removed.
    if (MOZ_LOG_TEST(sBackgroundTasksLog, mozilla::LogLevel::Info)) {
      nsAutoString path;
      if (NS_SUCCEEDED(sSingleton->mProfD->GetPath(path))) {
        MOZ_LOG(
            sBackgroundTasksLog, mozilla::LogLevel::Info,
            ("Removing profile: %s", NS_LossyConvertUTF16toASCII(path).get()));
      }
    }

    Unused << sSingleton->mProfD->Remove(/* aRecursive */ true);
  }

  sSingleton = nullptr;
}

BackgroundTasks* BackgroundTasks::GetSingleton() {
  if (!sSingleton) {
    // xpcshell doesn't set up background tasks: default to no background
    // task.
    Init(Nothing());
  }

  MOZ_RELEASE_ASSERT(sSingleton,
                     "BackgroundTasks singleton should have been initialized");

  return sSingleton.get();
}

already_AddRefed<BackgroundTasks> BackgroundTasks::GetSingletonAddRefed() {
  return RefPtr<BackgroundTasks>(GetSingleton()).forget();
}

Maybe<nsCString> BackgroundTasks::GetBackgroundTasks() {
  if (!XRE_IsParentProcess()) {
    return Nothing();
  }

  return GetSingleton()->mBackgroundTask;
}

bool BackgroundTasks::IsBackgroundTaskMode() {
  if (!XRE_IsParentProcess()) {
    return false;
  }

  return GetBackgroundTasks().isSome();
}

nsresult BackgroundTasks::CreateTemporaryProfileDirectory(
    const nsCString& aInstallHash, nsIFile** aFile) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv =
      GetSingleton()->CreateTemporaryProfileDirectoryImpl(aInstallHash, aFile);

  // Log whether the temporary profile was created.
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Warning,
            ("Failed to create temporary profile directory!"));
  } else {
    if (MOZ_LOG_TEST(sBackgroundTasksLog, mozilla::LogLevel::Info)) {
      nsAutoString path;
      if (aFile && *aFile && NS_SUCCEEDED((*aFile)->GetPath(path))) {
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Info,
                ("Created temporary profile directory: %s",
                 NS_LossyConvertUTF16toASCII(path).get()));
      }
    }
  }

  return rv;
}

bool BackgroundTasks::IsUsingTemporaryProfile() {
  return sSingleton && sSingleton->mProfD;
}

nsresult BackgroundTasks::RunBackgroundTask(nsICommandLine* aCmdLine) {
  Maybe<nsCString> task = GetBackgroundTasks();
  if (task.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIBackgroundTasksManager> manager =
      do_GetService("@mozilla.org/backgroundtasksmanager;1");

  MOZ_RELEASE_ASSERT(manager, "Could not get background tasks manager service");

  NS_ConvertASCIItoUTF16 name(task.ref().get());
  Unused << manager->RunBackgroundTaskNamed(name, aCmdLine);

  return NS_OK;
}

bool BackgroundTasks::IsUpdatingTaskName(const nsCString& aName) {
  return aName.EqualsLiteral("backgroundupdate") ||
         aName.EqualsLiteral("shouldprocessupdates");
}

nsresult BackgroundTasks::CreateTemporaryProfileDirectoryImpl(
    const nsCString& aInstallHash, nsIFile** aFile) {
  if (mBackgroundTask.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;

  nsCOMPtr<nsIFile> file;
  if (mProfD) {
    rv = mProfD->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // We don't have the directory service at this point.
    rv = GetSpecialSystemDirectory(OS_TemporaryDirectory, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString profilePrefix =
        nsPrintfCString("%sBackgroundTask-%s-%s", MOZ_APP_VENDOR,
                        aInstallHash.get(), mBackgroundTask.ref().get());

    // Windows file cleanup is unreliable, so let's take a moment to clean up
    // any prior background task profiles. We can continue if there was an error
    // as creating a new temporary profile does not require cleaning up the old.
    rv = RemoveStaleTemporaryProfileDirectories(file, profilePrefix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Warning,
              ("Error cleaning up stale temporary profile directories."));
    }

    // The base path is /tmp/[vendor]BackgroundTask-[pathHash]-[taskName].
    rv = file->AppendNative(profilePrefix);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create a unique profile directory.  This can fail if there are too many
    // (thousands) of existing directories, which is unlikely to happen.
    rv = file->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->Clone(getter_AddRefs(mProfD));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  file.forget(aFile);
  return NS_OK;
}

nsresult BackgroundTasks::RemoveStaleTemporaryProfileDirectories(
    nsIFile* const aRoot, const nsCString& aPrefix) {
  nsresult rv;

  if (MOZ_LOG_TEST(sBackgroundTasksLog, LogLevel::Info)) {
    nsAutoString path;
    if (NS_SUCCEEDED(aRoot->GetPath(path))) {
      MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Info,
              ("Checking \"%s\" for stale profiles matching \"%s\".",
               NS_LossyConvertUTF16toASCII(path.get()).get(), aPrefix.get()));
    }
  }

  // Check how old a background task should be before being deleted.
  PRTime timeoutMillis = 60 * 1000;  // Default to 1 minute.
  bool deleteAll = false;
  // MOZ_BACKGROUNDTASKS_PURGE_STALE_PROFILES = ["0"+ in ms, "always", "never"]
  nsAutoCString envTimeoutStr(
      PR_GetEnv("MOZ_BACKGROUNDTASKS_PURGE_STALE_PROFILES"));

  if (!envTimeoutStr.IsEmpty()) {
    int64_t envTimeoutMillis = envTimeoutStr.ToInteger64(&rv);
    if (NS_SUCCEEDED(rv)) {
      if (envTimeoutMillis >= 0) {
        timeoutMillis = envTimeoutMillis;
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Info,
                ("Setting stale profile age to %sms", envTimeoutStr.get()));
      } else {
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Warning,
                ("MOZ_BACKGROUNDTASKS_PURGE_STALE_PROFILES is set less than 0, "
                 "using default timeout instead."));
      }
    } else {
      if (envTimeoutStr.Equals("always")) {
        deleteAll = true;
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Info,
                ("Deleting profiles regardless of age."));
      } else if (envTimeoutStr.Equals("never")) {
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Info,
                ("Skipping cleanup of stale background task profiles."));
        return NS_OK;
      } else {
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Warning,
                ("MOZ_BACKGROUNDTASKS_PURGE_STALE_PROFILES is set to invalid "
                 "value, using default timeout instead."));
      }
    }
  }

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = aRoot->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> entry;
  int removedProfiles = 0;
  // Limit the number of stale temporary profiles we clean up so that we don't
  // timeout the background task.
  const int kMaxRemovedProfiles = 5;

  // Loop over the temporary directory entries, deleting folders matching our
  // profile prefix. Continue if there is an error interacting with the entry to
  // more reliably make progress on cleaning up stale temporary profiles.
  while (removedProfiles < kMaxRemovedProfiles &&
         NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(entry))) &&
         entry) {
    nsCString entryName;
    rv = entry->GetNativeLeafName(entryName);
    if (NS_FAILED(rv)) {
      continue;
    }

    // Find profile folders matching our prefix.
    if (aPrefix.Compare(entryName.get(), false, aPrefix.Length()) != 0) {
      continue;
    }

    if (!deleteAll) {
      // Skip profiles that were recently created to prevent deleting a profile
      // after creating the directory but before creating the lockfile.
      PRTime profileModifyTime;
      if (NS_FAILED(entry->GetLastModifiedTime(&profileModifyTime))) {
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Warning,
                ("Skipping deletion of %s, unable to retrieve when profile was "
                 "last modified.",
                 entryName.get()));
        continue;
      }
      PRTime now = PR_Now() / PR_USEC_PER_MSEC;
      // Timeout only needs to be large enough to prevent deleting a temporary
      // profile between it being created and locked.
      if (now - profileModifyTime < timeoutMillis) {
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Debug,
                ("Skipping deletion of %s, profile is not yet stale.",
                 entryName.get()));
        continue;
      }
    }

    // Check if the profile is locked. If successful drop the lock so we can
    // delete the folder. Background tasks' temporary profiles are not reused or
    // remembered once released, so we don't need to hold this lock while
    // deleting it.
    nsProfileLock lock;
    if (NS_FAILED(lock.Lock(entry, nullptr)) || NS_FAILED(lock.Unlock())) {
      MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Warning,
              ("Skipping deletion of %s, unable to lock/unlock profile.",
               entryName.get()));
      continue;
    }

    rv = entry->Remove(true);
    if (NS_FAILED(rv)) {
      if (MOZ_LOG_TEST(sBackgroundTasksLog, mozilla::LogLevel::Warning)) {
        nsAutoString path;
        if (NS_SUCCEEDED(entry->GetPath(path))) {
          MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Warning,
                  ("Error removing stale temporary profile directory: %s",
                   NS_LossyConvertUTF16toASCII(path).get()));
        }
      }

      continue;
    }

    if (MOZ_LOG_TEST(sBackgroundTasksLog, mozilla::LogLevel::Info)) {
      nsAutoString path;
      if (NS_SUCCEEDED(entry->GetPath(path))) {
        MOZ_LOG(sBackgroundTasksLog, mozilla::LogLevel::Info,
                ("Removed stale temporary profile directory: %s",
                 NS_LossyConvertUTF16toASCII(path).get()));
      }
    }

    removedProfiles++;
  }

  return NS_OK;
}

nsresult BackgroundTasks::GetIsBackgroundTaskMode(bool* result) {
  *result = mBackgroundTask.isSome();
  return NS_OK;
}

nsresult BackgroundTasks::BackgroundTaskName(nsAString& name) {
  name.SetIsVoid(true);
  if (mBackgroundTask.isSome()) {
    name.AssignASCII(mBackgroundTask.ref());
  }
  return NS_OK;
}

nsresult BackgroundTasks::OverrideBackgroundTaskNameForTesting(
    const nsAString& name) {
  if (name.IsVoid()) {
    mBackgroundTask = Nothing();
  } else {
    mBackgroundTask = Some(NS_LossyConvertUTF16toASCII(name));
  }
  return NS_OK;
}

StaticRefPtr<BackgroundTasks> BackgroundTasks::sSingleton;

LazyLogModule BackgroundTasks::sBackgroundTasksLog("BackgroundTasks");

}  // namespace mozilla
