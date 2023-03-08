/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  Management: "resource://gre/modules/Extension.jsm",
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
  const { XPIProvider } = ChromeUtils.import(
    "resource://gre/modules/addons/XPIProvider.jsm"
  );
  function listener(_evt, extension) {
    ok(
      !XPIProvider._closing,
      `Unxpected addon startup for "${extension.id}" after XPIProvider have been closed and shutting down`
    );
  }
  Management.on("startup", listener);
  promiseStartupManager();
  await promiseShutdownManager();

  info("Uninstall test extensions");
  Management.off("startup", listener);
  await promiseStartupManager();
  await Promise.all(extensions.map(ext => ext.awaitStartup));
  await Promise.all(
    extensions.map(ext => {
      return AddonManager.getAddonByID(ext.id).then(addon =>
        addon?.uninstall()
      );
    })
  );
  await promiseShutdownManager();
});

// Regression test for Bug 1799421.
add_task(async function test_late_XPIDB_load_rejected() {
  await AddonTestUtils.promiseStartupManager();

  // Mock a late XPIDB load and expect to be rejected.
  const { XPIProvider } = ChromeUtils.import(
    "resource://gre/modules/addons/XPIProvider.jsm"
  );
  const { XPIDatabase } = ChromeUtils.import(
    "resource://gre/modules/addons/XPIDatabase.jsm"
  );

  XPIProvider._closing = true;
  XPIDatabase._dbPromise = null;
  Assert.equal(
    await XPIDatabase.getAddonByID("test@addon"),
    null,
    "Expect a late getAddonByID call to be sucessfully resolved to null"
  );

  await Assert.rejects(
    XPIDatabase._dbPromise,
    /XPIDatabase.asyncLoadDB attempt after XPIProvider shutdown/,
    "Expect XPIDatebase._dbPromise to be set to the expected rejected promise"
  );
  // Cleanup AOM mocked status and shutdown AOM before exit test.
  XPIProvider._closing = false;
  XPIDatabase._dbPromise = null;
  await AddonTestUtils.promiseShutdownManager();
});

// Regression test for Bug 1799421.
//
// NOTE: this test calls Services.startup.advanceShutdownPhase
// with SHUTDOWN_PHASE_APPSHUTDOWNCONFIRMED to mock the scenario
// and so using promiseStartupManager/promiseShutdownManager will
// fail to bring the AddonManager back into a working status
// because it would be detected as too late on the shutdown path.
add_task(async function test_late_bootstrapscope_startup_rejected() {
  await AddonTestUtils.promiseStartupManager();

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: {
        gecko: { id: "test@addon" },
      },
    },
  });

  await extension.startup();
  const { addon } = extension;
  await addon.disable();
  // Mock a shutdown which already got to shutdown confirmed
  // and expect a rejection from trying to startup the BootstrapScope
  // too late on shutdown.
  Services.startup.advanceShutdownPhase(
    Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNCONFIRMED
  );

  await Assert.rejects(
    addon.enable(),
    /XPIProvider can't start bootstrap scope for test@addon after shutdown was already granted/,
    "Got the expected rejection on trying to enable the extension after shutdown granted"
  );

  info("Cleanup mocked late bootstrap scope before exit test");
  await AddonTestUtils.promiseShutdownManager();
});
