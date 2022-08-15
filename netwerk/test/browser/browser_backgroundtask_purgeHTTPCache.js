/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_startupCleanup() {
  Services.prefs.setBoolPref(
    "network.cache.shutdown_purge_in_background_task",
    true
  );
  let dir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  dir.append("cache2.2021-11-25-08-47-04.purge.bg_rm");
  Assert.equal(dir.exists(), false, `Folder ${dir.path} should not exist`);
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  Assert.equal(
    dir.exists(),
    true,
    `Folder ${dir.path} should have been created`
  );

  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");

  await TestUtils.waitForCondition(() => {
    return !dir.exists();
  });

  Assert.equal(
    dir.exists(),
    false,
    `Folder ${dir.path} should have been purged by background task`
  );
  Services.prefs.clearUserPref(
    "network.cache.shutdown_purge_in_background_task"
  );
});
