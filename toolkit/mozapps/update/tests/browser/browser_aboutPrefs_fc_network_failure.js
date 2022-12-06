/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates which fails,
// because it is impossible to connect to the update server.
add_task(async function aboutPrefs_foregroundCheck_network_failure() {
  let params = {
    baseURL: "https://localhost:7777",
  };
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "checkingFailed",
    },
  ]);
});
