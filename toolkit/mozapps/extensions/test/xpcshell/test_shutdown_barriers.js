/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  Management: "resource://gre/modules/Extension.jsm",
  XPIProvider: "resource://gre/modules/addons/XPIProvider.jsm",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

add_task(async function test_shutdown_barriers() {
  await promiseStartupManager();

  const ID = "thing@xpcshell.addons.mozilla.org";
  const VERSION = "1.42";

  let xpi = await createTempWebExtensionFile({
    manifest: {
      browser_specific_settings: { gecko: { id: ID } },
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

// Regression test for Bug 1814104.
add_task(async function test_wait_addons_startup_before_granting_quit() {
  await promiseStartupManager();

  const extensions = [];
  for (let i = 0; i < 20; i++) {
    extensions.push(
      ExtensionTestUtils.loadExtension({
        useAddonManager: "permanent",
        background() {},
        manifest: {
          browser_specific_settings: {
            gecko: { id: `test-extension-${i}@mozilla` },
          },
        },
      })
    );
  }

  info("Wait for all test extension to have been started once");
  await Promise.all(extensions.map(ext => ext.startup()));
  await promiseShutdownManager();

  info("Test early shutdown while enabled addons are still being started");

  Management.on("startup", (_evt, extension) => {
    ok(
      !XPIProvider._closing,
      `Unxpected addon startup for "${extension.id}" after XPIProvider have been closed and shutting down`
    );
  });
  promiseStartupManager();
  await promiseShutdownManager();
});
