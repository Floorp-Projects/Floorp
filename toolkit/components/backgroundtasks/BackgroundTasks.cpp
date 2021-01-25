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

nsresult BackgroundTasks::GetOrCreateTemporaryProfileDirectoryImpl(
    nsIFile** aFile) {
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

    // TODO: mix in the application hash and perhaps add some additional
    // randomness.
    rv = file->AppendNative(
        nsPrintfCString("backgroundtask-%s", mBackgroundTask.ref().get()));
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure that the profile path exists and it's a directory.
    bool exists;
    rv = file->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = file->Create(nsIFile::DIRECTORY_TYPE, 0700);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      bool isDir;
      rv = file->IsDirectory(&isDir);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!isDir) {
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
      }
    }

    rv = file->Clone(getter_AddRefs(mProfD));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsString path;
  file->GetPath(path);

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
