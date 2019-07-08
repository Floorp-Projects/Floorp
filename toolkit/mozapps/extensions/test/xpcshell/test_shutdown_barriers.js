/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

add_task(async function test_shutdown_barriers() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "66");

  await promiseStartupManager();

  const ID = "thing@xpcshell.addons.mozilla.org";
  const VERSION = "1.42";

  let xpi = await createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id: ID } },
      version: VERSION,
    },
  });

  await AddonManager.installTemporaryAddon(xpi);

  let blockersComplete = 0;
  AddonManager.beforeShutdown.addBlocker("Before shutdown", async () => {
    equal(blockersComplete, 0, "No blockers have run yet");

    // Delay a bit to make sure this doesn't succeed because of a timing
    // fluke.
    await delay(1000);

    let addon = await AddonManager.getAddonByID(ID);
    checkAddon(ID, addon, {
      version: VERSION,
      userDisabled: false,
      appDisabled: false,
    });

    await addon.disable();

    // Delay again for the same reasons.
    await delay(1000);

    addon = await AddonManager.getAddonByID(ID);
    checkAddon(ID, addon, {
      version: VERSION,
      userDisabled: true,
      appDisabled: false,
    });

    blockersComplete++;
  });
  AddonManagerPrivate.finalShutdown.addBlocker("Before shutdown", async () => {
    equal(blockersComplete, 1, "Before shutdown blocker has run");

    // Should probably try to access XPIDatabase here to make sure it
    // doesn't work correctly, but it's a bit hairy.

    // Delay a bit to make sure this doesn't succeed because of a timing
    // fluke.
    await delay(1000);

    blockersComplete++;
  });

  await promiseShutdownManager();
  equal(
    blockersComplete,
    2,
    "Both shutdown blockers ran before manager shutdown completed"
  );
});
