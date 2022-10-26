/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that while a download is in-progress, calling `AppUpdater.stop()` while
// in the checking state causes the interface to return to the `NEVER_CHECKED`
// state.
// This is less a test of the About dialog than of AppUpdater, but it's easier
// to test it via the About dialog just because there is already a testing
// framework for the About dialog.
add_task(async function aboutDialog_AppUpdater_stop_checking() {
  let params = { queryString: "&noUpdates=1" };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      // Omit the continue file to keep us in the checking state.
    },
    aboutDialog => {
      aboutDialog.gAppUpdater._appUpdater.stop();
    },
    {
      panelId: "checkForUpdates",
    },
  ]);

  // Ideally this would go in a cleanup function. But this needs to happen
  // before any other cleanup functions and for some reason cleanup functions
  // do not always seem to execute in reverse registration order.
  dump("Cleanup: Waiting for checking to finish.\n");
  await continueFileHandler(CONTINUE_CHECK);
});
