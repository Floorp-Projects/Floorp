/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BackgroundTasks.h"

#include "nsIBackgroundTasksManager.h"
#include "nsIFile.h"
#include "nsImportModule.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "SpecialSystemDirectory.h"

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

  if (sSingleton->mProfD) {
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

    // The base path is /tmp/[vendor]BackgroundTask-[pathHash]-[taskName].
    rv = file->AppendNative(nsPrintfCString("%sBackgroundTask-%s-%s",
                                            MOZ_APP_VENDOR, aInstallHash.get(),
                                            mBackgroundTask.ref().get()));
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
