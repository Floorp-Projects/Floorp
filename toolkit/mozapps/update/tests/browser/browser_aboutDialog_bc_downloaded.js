/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog background check for updates
// with the update downloaded.
add_task(async function aboutDialog_backgroundCheck_downloaded() {
  let updateParams = "";
  await runAboutDialogUpdateTest(updateParams, true, [
    {
      panelId: "apply",
      checkActiveUpdate: {state: STATE_PENDING},
      continueFile: null,
    },
  ]);
});
