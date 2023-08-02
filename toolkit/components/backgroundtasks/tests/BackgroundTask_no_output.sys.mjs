/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export async function runBackgroundTask(commandLine) {
  // Exact same behaviour as `unique_profile`, but with a task name
  // that is recognized as a task that should produce no output.
  const taskModule = ChromeUtils.importESModule(
    "resource://testing-common/backgroundtasks/BackgroundTask_unique_profile.sys.mjs"
  );
  return taskModule.runBackgroundTask(commandLine);
}
