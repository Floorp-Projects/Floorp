/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences background check for updates
// with the update downloaded.
add_task(async function aboutPrefs_backgroundCheck_downloaded() {
  let updateParams = "";
  await runAboutPrefsUpdateTest(updateParams, true, [
    {
      panelId: "apply",
      checkActiveUpdate: {state: STATE_PENDING},
      continueFile: null,
    },
  ]);
});
