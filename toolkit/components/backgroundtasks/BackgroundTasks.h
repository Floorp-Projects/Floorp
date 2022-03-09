/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BackgroundTasks_h
#define mozilla_BackgroundTasks_h

#include "nsCOMPtr.h"
#include "nsIBackgroundTasks.h"
#include "nsISupports.h"
#include "nsString.h"

#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPtr.h"

class nsICommandLine;
class nsIFile;

namespace mozilla {

class BackgroundTasks final : public nsIBackgroundTasks {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBACKGROUNDTASKS

 public:
  explicit BackgroundTasks(Maybe<nsCString> aBackgroundTask);

  static void Init(Maybe<nsCString> aBackgroundTask);

  static void Shutdown();

  /**
   * Return a raw pointer to the singleton instance.  Use this accessor in C++
   * code that just wants to call a method on the instance, but does not need to
   * hold a reference.
   */
  static BackgroundTasks* GetSingleton();

  /**
   * Return an addRef'd pointer to the singleton instance. This is used by the
   * XPCOM constructor that exists to support usage from JS.
   */
  static already_AddRefed<BackgroundTasks> GetSingletonAddRefed();

  static Maybe<nsCString> GetBackgroundTasks();

  static bool IsBackgroundTaskMode();

  static nsresult CreateTemporaryProfileDirectory(const nsCString& aInstallHash,
                                                  nsIFile** aFile);

  static bool IsUsingTemporaryProfile();

  static nsresult RunBackgroundTask(nsICommandLine* aCmdLine);

  /**
   * Whether the given task name should process updates.  Most tasks should not
   * process updates to avoid Firefox being updated unexpectedly.
   *
   * Right now, we only process updates for the `backgroundupdate` task and the
   * test-only `shouldprocessupdates` task.
   */
  static bool IsUpdatingTaskName(const nsCString& aName);

 protected:
  static StaticRefPtr<BackgroundTasks> sSingleton;
  static LazyLogModule sBackgroundTasksLog;

  Maybe<nsCString> mBackgroundTask;
  nsCOMPtr<nsIFile> mProfD;

  nsresult CreateTemporaryProfileDirectoryImpl(const nsCString& aInstallHash,
                                               nsIFile** aFile);

  virtual ~BackgroundTasks() = default;
};

}  // namespace mozilla

#endif  // mozilla_BackgroundTasks_h
