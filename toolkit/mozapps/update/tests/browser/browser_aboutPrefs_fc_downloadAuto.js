/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates
// with an automatic download.
add_task(async function aboutPrefs_foregroundCheck_downloadAuto() {
  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let updateParams = "&invalidCompleteSize=1";
  await runAboutPrefsUpdateTest(updateParams, false, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: {state: STATE_DOWNLOADING},
      continueFile: CONTINUE_DOWNLOAD,
    },
    {
      panelId: "apply",
      checkActiveUpdate: {state: STATE_PENDING},
      continueFile: null,
    },
  ]);
});
