/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BackgroundTasks_h
#define mozilla_BackgroundTasks_h

#include "nsCOMPtr.h"
#include "nsIBackgroundTasks.h"
#include "nsIBackgroundTasksManager.h"
#include "nsICommandLine.h"
#include "nsIFile.h"
#include "nsISupports.h"
#include "nsImportModule.h"
#include "nsString.h"
#include "nsXULAppAPI.h"

#include "mozilla/LateWriteChecks.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"

#include "prenv.h"

namespace mozilla {

class BackgroundTasks final : public nsIBackgroundTasks {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBACKGROUNDTASKS

 public:
  explicit BackgroundTasks(Maybe<nsCString> aBackgroundTask)
      : mBackgroundTask(aBackgroundTask) {}

  static void Init(Maybe<nsCString> aBackgroundTask) {
    MOZ_RELEASE_ASSERT(XRE_IsParentProcess());

    MOZ_RELEASE_ASSERT(!sSingleton,
                       "BackgroundTasks singleton already initialized");
    // The singleton will be cleaned up by `Shutdown()`.
    sSingleton = new BackgroundTasks(aBackgroundTask);
  }

  static void Shutdown() {
    MOZ_RELEASE_ASSERT(XRE_IsParentProcess());

    if (!sSingleton) {
      return;
    }

    if (sSingleton->mProfD) {
      AutoSuspendLateWriteChecks suspend;

      mozilla::Unused << sSingleton->mProfD->Remove(/* aRecursive */ true);
    }

    sSingleton = nullptr;
  }

  /**
   * Return a raw pointer to the singleton instance.  Use this accessor in C++
   * code that just wants to call a method on the instance, but does not need to
   * hold a reference.
   */
  static BackgroundTasks* GetSingleton() {
    if (!sSingleton) {
      // xpcshell doesn't set up background tasks: default to no background
      // task.
      Init(Nothing());
    }

    MOZ_RELEASE_ASSERT(
        sSingleton, "BackgroundTasks singleton should have been initialized");

    return sSingleton.get();
  }

  /**
   * Return an addRef'd pointer to the singleton instance. This is used by the
   * XPCOM constructor that exists to support usage from JS.
   */
  static already_AddRefed<BackgroundTasks> GetSingletonAddRefed() {
    return RefPtr<BackgroundTasks>(GetSingleton()).forget();
  }

  static const Maybe<nsCString> GetBackgroundTasks() {
    if (!XRE_IsParentProcess()) {
      return Nothing();
    }

    return GetSingleton()->mBackgroundTask;
  }

  static bool IsBackgroundTaskMode() {
    if (!XRE_IsParentProcess()) {
      return false;
    }

    return GetBackgroundTasks().isSome();
  }

  static nsresult CreateTemporaryProfileDirectory(const nsCString& aInstallHash,
                                                  nsIFile** aFile) {
    if (!XRE_IsParentProcess()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    return GetSingleton()->CreateTemporaryProfileDirectoryImpl(aInstallHash,
                                                               aFile);
  }

  static bool IsUsingTemporaryProfile() {
    return sSingleton && sSingleton->mProfD;
  }

  static nsresult RunBackgroundTask(nsICommandLine* aCmdLine) {
    Maybe<nsCString> task = GetBackgroundTasks();
    if (task.isNothing()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIBackgroundTasksManager> manager =
        do_ImportModule("resource://gre/modules/BackgroundTasksManager.jsm",
                        "BackgroundTasksManager");

    NS_ENSURE_TRUE(manager, NS_ERROR_FAILURE);

    NS_ConvertASCIItoUTF16 name(task.ref().get());
    Unused << manager->RunBackgroundTaskNamed(name, aCmdLine);

    return NS_OK;
  }

 protected:
  static StaticRefPtr<BackgroundTasks> sSingleton;

  Maybe<nsCString> mBackgroundTask;
  nsCOMPtr<nsIFile> mProfD;

  nsresult CreateTemporaryProfileDirectoryImpl(const nsCString& aInstallHash,
                                               nsIFile** aFile);

  virtual ~BackgroundTasks() = default;
};

}  // namespace mozilla

#endif  // mozilla_BackgroundTasks_h
