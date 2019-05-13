/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates
// without the ability to apply updates.
add_task(async function aboutPrefs_foregroundCheck_cantApply() {
  lockWriteTestFile();

  let params = {};
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "manualUpdate",
      checkActiveUpdate: null,
      continueFile: null,
    },
  ]);
});
