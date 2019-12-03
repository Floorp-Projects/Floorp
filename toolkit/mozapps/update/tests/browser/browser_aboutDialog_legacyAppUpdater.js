/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This makes sure the legacy app updater is used when the new one is preffed
// off.
add_task(async function aboutDialog_legacyAppUpdater() {
  let aboutDialog = await waitForAboutDialog();
  registerCleanupFunction(() => {
    aboutDialog.close();
  });
  await TestUtils.waitForCondition(
    () => aboutDialog.gAppUpdater,
    "Wait for gAppUpdater to be set"
  );
  Assert.ok(
    !aboutDialog.gAppUpdater._appUpdater,
    "gAppUpdater._appUpdater should not be defined when using the legacy app updater"
  );
});
