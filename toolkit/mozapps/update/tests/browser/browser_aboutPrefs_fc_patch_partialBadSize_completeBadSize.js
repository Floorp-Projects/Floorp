/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates
// with a partial bad size patch and a complete bad size patch.
add_task(
  async function aboutPrefs_foregroundCheck_partialBadSize_completeBadSize() {
    let downloadInfo = [];
    if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
      downloadInfo[0] = { patchType: "partial", bitsResult: gBadSizeResult };
      downloadInfo[1] = {
        patchType: "partial",
        internalResult: gBadSizeResult,
      };
      downloadInfo[2] = { patchType: "complete", bitsResult: gBadSizeResult };
      downloadInfo[3] = {
        patchType: "complete",
        internalResult: gBadSizeResult,
      };
    } else {
      downloadInfo[0] = {
        patchType: "partial",
        internalResult: gBadSizeResult,
      };
      downloadInfo[1] = {
        patchType: "complete",
        internalResult: gBadSizeResult,
      };
    }

    let params = { queryString: "&invalidPartialSize=1&invalidCompleteSize=1" };
    await runAboutPrefsUpdateTest(params, [
      {
        panelId: "checkingForUpdates",
        checkActiveUpdate: null,
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
        checkActiveUpdate: null,
        continueFile: null,
      },
    ]);
  }
);
