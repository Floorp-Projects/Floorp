/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences foreground check for updates which fails because
// the browser is in offline mode and `localhost` cannot be resolved.
add_task(async function aboutPrefs_foregroundCheck_network_offline() {
  // avoid that real network connectivity changes influence the test execution
  Services.io.manageOfflineStatus = false;
  Services.io.offline = true;

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.disable-localhost-when-offline", true],
      ["network.dns.offline-localhost", false],
    ],
  });

  await runAboutPrefsUpdateTest({}, [
    {
      panelId: "checkingFailed",
    },
  ]);
});
