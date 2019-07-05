/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates
// with an unsupported update.
add_task(async function aboutPrefs_foregroundCheck_unsupported() {
  let params = { queryString: "&unsupported=1" };
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "unsupportedSystem",
      checkActiveUpdate: null,
      continueFile: null,
    },
  ]);
});
