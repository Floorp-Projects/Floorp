/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog foreground check for updates
// with a complete bad size patch.
add_task(async function aboutDialog_foregroundCheck_completeBadSize() {
  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = {patchType: "complete",
                       bitsResult: gBadSizeResult};
    downloadInfo[1] = {patchType: "complete",
                       internalResult: gBadSizeResult};
  } else {
    downloadInfo[0] = {patchType: "complete",
                       internalResult: gBadSizeResult};
  }

  let params = {queryString: "&completePatchOnly=1&invalidCompleteSize=1"};
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
      panelId: "downloadFailed",
      checkActiveUpdate: null,
      continueFile: null,
    },
  ]);
});
