/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates
// with another application instance handling updates.
add_task(async function aboutPrefs_foregroundCheck_otherInstance() {
  setOtherInstanceHandlingUpdates();

  let params = {};
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "otherInstanceHandlingUpdates",
      checkActiveUpdate: null,
      continueFile: null,
    },
  ]);
});
