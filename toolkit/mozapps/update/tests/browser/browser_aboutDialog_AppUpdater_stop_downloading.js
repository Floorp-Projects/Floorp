/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that while a download is in-progress, calling `AppUpdater.stop()` during
// the downloading state causes the interface to return to the `NEVER_CHECKED`
// state.
// This is less a test of the About dialog than of AppUpdater, but it's easier
// to test it via the About dialog just because there is already a testing
// framework for the About dialog.
add_task(async function aboutDialog_AppUpdater_stop_downloading() {
  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = {
    queryString: "&useSlowDownloadMar=1&invalidCompleteSize=1",
    backgroundUpdate: true,
    waitForUpdateState: STATE_DOWNLOADING,
  };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      // Omit continue file to keep the UI in the downloading state.
    },
    aboutDialog => {
      aboutDialog.gAppUpdater._appUpdater.stop();
    },
    {
      panelId: "checkForUpdates",
      // The update will still be in the downloading state even though
      // AppUpdater has stopped because stopping AppUpdater doesn't stop the
      // Application Update Service from continuing with the update.
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      expectedStateOverride: Ci.nsIApplicationUpdateService.STATE_DOWNLOADING,
    },
  ]);

  // Ideally this would go in a cleanup function. But this needs to happen
  // before any other cleanup functions and for some reason cleanup functions
  // do not always seem to execute in reverse registration order.
  dump("Cleanup: Waiting for downloading to finish.\n");
  await continueFileHandler(CONTINUE_DOWNLOAD);
  if (gAUS.currentState == Ci.nsIApplicationUpdateService.STATE_DOWNLOADING) {
    await gAUS.stateTransition;
  }
});
