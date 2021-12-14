/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_backgroundtask_shouldprocessupdates() {
  // The task returns 81 if we should process updates, i.e.,
  // !ShouldNotProcessUpdates(), <= 80 otherwise.  xpcshell itself counts as an
  // instance, so the background task will see it and think another instance is
  // running.  N.b.: this isn't as robust as it could be: running Firefox
  // instances and parallel tests interact here (mostly harmlessly).
  //
  // Since the behaviour under test (ShouldNotProcessUpdates()) happens at startup,
  // we can't easily change the lock location of the background task.

  // `shouldprocessupdates` is an updating task, but there is another instance
  // running, so we should not process updates.
  let exitCode = await do_backgroundtask("shouldprocessupdates", {
    extraArgs: ["--test-process-updates"],
  });
  Assert.equal(80, exitCode);

  // If we change our lock location, the background task won't see our instance
  // running.
  let file = do_get_profile();
  file.append("customExePath");
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  syncManager.resetLock(file);

  // Since we've changed the xpcshell executable name, the background task won't
  // see us and think another instance is running.  This time, there is no
  // reason to not process updates.
  exitCode = await do_backgroundtask("shouldprocessupdates");
  Assert.equal(81, exitCode);

  // `shouldnotprocessupdates` is not a recognized updating task, so we should
  // not process updates.
  exitCode = await do_backgroundtask("shouldnotprocessupdates", {
    extraArgs: ["--test-process-updates"],
  });
  Assert.equal(78, exitCode);
});
