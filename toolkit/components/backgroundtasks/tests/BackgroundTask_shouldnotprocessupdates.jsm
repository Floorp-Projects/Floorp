/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

async function runBackgroundTask(commandLine) {
  // Exact same behaviour as `shouldprocessupdates`, but with a task name that
  // is not recognized as a task that should process updates.
  const taskModule = ChromeUtils.import(
    "resource://testing-common/backgroundtasks/BackgroundTask_shouldprocessupdates.jsm"
  );
  return taskModule.runBackgroundTask(commandLine);
}
