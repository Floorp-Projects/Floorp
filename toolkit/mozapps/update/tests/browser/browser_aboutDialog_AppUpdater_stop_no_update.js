/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that while a download is in-progress, calling `AppUpdater.stop()` while
// in the "noUpdatesFound" state doesn't cause a shift to any other state, such
// as internal error.
// This is less a test of the About dialog than of AppUpdater, but it's easier
// to test it via the About dialog just because there is already a testing
// framework for the About dialog.
add_task(async function aboutDialog_AppUpdater_stop_no_update() {
  let params = { queryString: "&noUpdates=1" };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "noUpdatesFound",
    },
    aboutDialog => {
      aboutDialog.gAppUpdater._appUpdater.stop();
    },
    {
      panelId: "noUpdatesFound",
    },
  ]);
});
