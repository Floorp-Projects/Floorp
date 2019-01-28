/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates
// with a partial bad size patch.
add_task(async function aboutPrefs_foregroundCheck_partialBadSize() {
  let updateParams = "&partialPatchOnly=1&invalidPartialSize=1";
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
      panelId: "downloadFailed",
      checkActiveUpdate: null,
      continueFile: null,
    },
  ]);
});
