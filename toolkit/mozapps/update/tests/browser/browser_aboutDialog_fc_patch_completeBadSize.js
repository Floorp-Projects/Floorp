/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog foreground check for updates
// with a complete bad size patch.
add_task(async function aboutDialog_foregroundCheck_completeBadSize() {
  let updateParams = "&completePatchOnly=1&invalidCompleteSize=1";
  await runAboutDialogUpdateTest(updateParams, false, [
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
      panelId: "downloadFailed",
      checkActiveUpdate: null,
      continueFile: null,
    },
  ]);
});
