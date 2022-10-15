/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";

/**
 * Return 0 (success) if the given absolute file path exists, 11
 * (failure) otherwise.
 */
export function runBackgroundTask(commandLine) {
  let path = commandLine.getArgument(0);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);

  let exitCode;
  let exists = file.exists();
  if (exists) {
    exitCode = EXIT_CODE.SUCCESS;
  } else {
    exitCode = 11;
  }

  console.error(
    `runBackgroundTask: '${path}' exists: ${exists}; ` +
      `exiting with status ${exitCode}`
  );
  return exitCode;
}
