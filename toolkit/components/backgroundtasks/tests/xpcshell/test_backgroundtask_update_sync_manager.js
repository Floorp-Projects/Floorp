/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_backgroundtask_update_sync_manager() {
  // The task returns 80 if another instance is running, 81 otherwise.  xpcshell
  // itself counts as an instance, so the background task will see it and think
  // another instance is running.
  //
  // This can also be achieved by overriding directory providers, but
  // that's not particularly robust in the face of parallel testing.
  // Doing it this way exercises `resetLock` with a path.
  let exitCode = await do_backgroundtask("update_sync_manager", {
    extraArgs: [Services.dirsvc.get("XREExeF", Ci.nsIFile).path],
  });
  Assert.equal(80, exitCode, "Another instance is running");

  // If we tell the backgroundtask to use a unique appPath, the
  // background task won't see any other instances running.
  let file = do_get_profile();
  file.append("customExePath");
  file.append("customExe");

  exitCode = await do_backgroundtask("update_sync_manager", {
    extraArgs: [file.path],
  });
  Assert.equal(81, exitCode, "No other instance is running");

  let upperCaseFile = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  upperCaseFile.initWithPath(
    Services.dirsvc.get("XREExeF", Ci.nsIFile).path.toUpperCase()
  );
  if (upperCaseFile.exists()) {
    // The uppercased path can still be used to access the exe, indicating a
    // case-insensitive filesystem (as is usual on Windows and macOS), so path
    // normalization can be tested.
    exitCode = await do_backgroundtask("update_sync_manager", {
      extraArgs: [upperCaseFile.path],
    });
    Assert.equal(80, exitCode, "Another instance is running");
  }
});
