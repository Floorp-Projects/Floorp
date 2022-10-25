/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that while a download is in-progress, calling `AppUpdater.stop()` while
// in the "downloadFailed" state doesn't cause a shift to any other state, such
// as internal error.
// This is less a test of the About dialog than of AppUpdater, but it's easier
// to test it via the About dialog just because there is already a testing
// framework for the About dialog.
add_task(async function aboutDialog_AppUpdater_stop_download_failed() {
  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = { patchType: "complete", bitsResult: gBadSizeResult };
    downloadInfo[1] = { patchType: "complete", internalResult: gBadSizeResult };
  } else {
    downloadInfo[0] = { patchType: "complete", internalResult: gBadSizeResult };
  }

  let params = { queryString: "&completePatchOnly=1&invalidCompleteSize=1" };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    {
      panelId: "downloadFailed",
    },
    aboutDialog => {
      aboutDialog.gAppUpdater._appUpdater.stop();
    },
    {
      panelId: "downloadFailed",
    },
  ]);
});
