/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog foreground check for updates
// with a partial bad size patch and a complete patch.
add_task(async function aboutDialog_foregroundCheck_partialBadSize_complete() {
  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = {patchType: "partial",
                       bitsResult: gBadSizeResult};
    downloadInfo[1] = {patchType: "partial",
                       internalResult: gBadSizeResult};
    downloadInfo[2] = {patchType: "complete",
                       bitsResult: "0"};
  } else {
    downloadInfo[0] = {patchType: "partial",
                       internalResult: gBadSizeResult};
    downloadInfo[1] = {patchType: "complete",
                       internalResult: "0"};
  }

  let params = {queryString: "&invalidPartialSize=1"};
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: {state: STATE_DOWNLOADING},
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    {
      panelId: "apply",
      checkActiveUpdate: {state: STATE_PENDING},
      continueFile: null,
    },
  ]);
});
