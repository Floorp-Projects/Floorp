/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This makes sure the new app updater is used when preffed on.
add_task(async function aboutDialog_newAppUpdater() {
  let aboutDialog = await waitForAboutDialog();
  registerCleanupFunction(() => {
    aboutDialog.close();
  });
  await TestUtils.waitForCondition(
    () => aboutDialog.gAppUpdater,
    "Wait for gAppUpdater to be set"
  );
  Assert.ok(
    aboutDialog.gAppUpdater._appUpdater,
    "gAppUpdater._appUpdater should be defined when using the new app updater"
  );
});
