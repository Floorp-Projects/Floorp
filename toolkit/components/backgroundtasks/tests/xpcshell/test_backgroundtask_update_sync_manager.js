/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_backgroundtask_update_sync_manager() {
  // The task returns 80 if another instance is running, 81 otherwise.  xpcshell
  // itself counts as an instance, so the background task will see it and think
  // another instance is running.  N.b.: this isn't as robust as it could be:
  // running Firefox instances and parallel tests interact here (mostly
  // harmlessly).
  let exitCode = await do_backgroundtask("update_sync_manager");
  Assert.equal(80, exitCode);

  // This functionality is copied from tests in `toolkit/mozapps/update/tests`.
  let dirProvider = {
    getFile: function AGP_DP_getFile(aProp, aPersistent) {
      // Set the value of persistent to false so when this directory provider is
      // unregistered it will revert back to the original provider.
      aPersistent.value = false;
      // The sync manager only needs XREExeF, so that's all we provide.
      if (aProp == "XREExeF") {
        let file = do_get_profile();
        file.append("customExePath");
        return file;
      }
      return null;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };
  let ds = Services.dirsvc.QueryInterface(Ci.nsIDirectoryService);
  ds.QueryInterface(Ci.nsIProperties).undefine("XREExeF");
  ds.registerProvider(dirProvider);

  try {
    // Now that we've overridden the directory provider, the name of the update
    // lock needs to be changed to match the overridden path.
    let syncManager = Cc[
      "@mozilla.org/updates/update-sync-manager;1"
    ].getService(Ci.nsIUpdateSyncManager);
    syncManager.resetLock();

    // The task returns 80 if another instance is running, 81 otherwise.  Since
    // we've changed the xpcshell executable name, the background task won't see
    // us and think another instance is running.  N.b.: this generally races
    // with other parallel tests and in the future it could be made more robust.
    exitCode = await do_backgroundtask("update_sync_manager");
    Assert.equal(81, exitCode);
  } finally {
    ds.unregisterProvider(dirProvider);
  }
});
