/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

async function runBackgroundTask(commandLine) {
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );

  if (commandLine.length) {
    let appPath = commandLine.getArgument(0);
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(appPath);
    syncManager.resetLock(file);
  }

  let exitCode = syncManager.isOtherInstanceRunning() ? 80 : 81;
  console.error(
    `runBackgroundTask: update_sync_manager exiting with exitCode ${exitCode}`
  );

  return exitCode;
}
