/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that while a download is in-progress, calling `AppUpdater.stop()` while
// in the "ready for restart" state doesn't cause a shift to any other state,
// such as internal error.
// This is less a test of the About dialog than of AppUpdater, but it's easier
// to test it via the About dialog just because there is already a testing
// framework for the About dialog.
add_task(async function aboutDialog_AppUpdater_stop_ready_for_restart() {
  let params = { backgroundUpdate: true, waitForUpdateState: STATE_PENDING };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_PENDING },
    },
    aboutDialog => {
      aboutDialog.gAppUpdater._appUpdater.stop();
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_PENDING },
    },
  ]);
});
