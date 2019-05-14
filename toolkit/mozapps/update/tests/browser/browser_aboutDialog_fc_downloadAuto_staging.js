/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog foreground check for updates
// with an automatic download and update staging.
add_task(async function aboutDialog_foregroundCheck_downloadAuto_staging() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });

  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = {patchType: "partial",
                       bitsResult: "0"};
  } else {
    downloadInfo[0] = {patchType: "partial",
                       internalResult: "0"};
  }

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = {queryString: "&invalidCompleteSize=1"};
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
      panelId: "applying",
      checkActiveUpdate: {state: STATE_PENDING},
      continueFile: CONTINUE_STAGING,
    },
    {
      panelId: "apply",
      checkActiveUpdate: {state: STATE_APPLIED},
      continueFile: null,
    },
  ]);
});
