/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog foreground check for updates which fails, because the
// browser is in offline mode and `localhost` cannot be resolved.
add_task(async function aboutDialog_foregroundCheck_network_offline() {
  info("[OFFLINE] setting Services.io.offline (do not forget to reset it!)");
  // avoid that real network connectivity changes influence the test execution
  Services.io.manageOfflineStatus = false;
  Services.io.offline = true;
  registerCleanupFunction(() => {
    info("[ONLINE] Resetting Services.io.offline");
    Services.io.offline = false;
    Services.io.manageOfflineStatus = true;
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.disable-localhost-when-offline", true],
      ["network.dns.offline-localhost", false],
    ],
  });

  await runAboutDialogUpdateTest({}, [
    {
      panelId: "checkingFailed",
    },
  ]);
});
