/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintfCString.h"
#include "SpecialSystemDirectory.h"

#include "mozilla/BackgroundTasks.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Maybe.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(BackgroundTasks, nsIBackgroundTasks);

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

mozilla::StaticRefPtr<BackgroundTasks> BackgroundTasks::sSingleton;

}  // namespace mozilla
