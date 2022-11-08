/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_BACKGROUNDTASKS_BACKGROUNDTASKSRUNNER_H_
#define TOOLKIT_COMPONENTS_BACKGROUNDTASKS_BACKGROUNDTASKSRUNNER_H_

#include "nsString.h"

namespace mozilla {

class BackgroundTasksRunner final {
 public:
  /**
   * Runs a background process in an independent detached process. Any process
   * opened by this function can outlive the main process.
   *
   * This function is thread-safe.
   *
   * @param aTaskName The name of the background task.
   *                  (BackgroundTask_{name}.sys.mjs)
   * @param aArgs The arguments that will be passed to the task process. Any
   *              needed escape will happen automatically.
   */
  static nsresult RunInDetachedProcess(const nsACString& aTaskName,
                                       const nsTArray<nsCString>& aArgs);

  /**
   * Runs removeDirectory background task.
   * `toolkit.background_tasks.remove_directory.testing.sleep_ms` can be set to
   * make it wait for the given milliseconds for testing purpose.
   *
   * See BackgroundTask_removeDirectory.sys.mjs for details about the arguments.
   */
  static nsresult RemoveDirectoryInDetachedProcess(
      const nsCString& aParentDirPath, const nsCString& aChildDirName,
      const nsCString& aSecondsToWait, const nsCString& aOtherFoldersSuffix);
};

}  // namespace mozilla

#endif  // TOOLKIT_COMPONENTS_BACKGROUNDTASKS_BACKGROUNDTASKSRUNNER_H_
