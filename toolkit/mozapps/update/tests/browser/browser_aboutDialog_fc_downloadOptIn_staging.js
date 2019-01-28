/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog foreground check for updates
// with a manual download and update staging.
add_task(async function aboutDialog_foregroundCheck_downloadOptIn_staging() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });
  await UpdateUtils.setAppUpdateAutoEnabled(false);

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let updateParams = "&invalidCompleteSize=1";
  await runAboutDialogUpdateTest(updateParams, false, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloadAndInstall",
      checkActiveUpdate: null,
      continueFile: null,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: {state: STATE_DOWNLOADING},
      continueFile: CONTINUE_DOWNLOAD,
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
