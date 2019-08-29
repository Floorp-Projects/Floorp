"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

// Automatically start the background page after restarting the AddonManager.
Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  false
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

const TEST_ADDON_ID = "@some-permanent-test-addon";

// Load a permanent extension that eventually unloads the extension immediately
// after add-on startup, to set the stage as a regression test for bug 1575190.
add_task(async function setup_wrapper() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      applications: { gecko: { id: TEST_ADDON_ID } },
    },
    background() {
      browser.test.sendMessage("started_up");
    },
  });

  await AddonTestUtils.promiseStartupManager();
  await extension.startup();
  await AddonTestUtils.promiseShutdownManager();

  // Check message because it is expected to be received while `startup()` was
  // pending resolution.
  info("Awaiting expected started_up message 1");
  await extension.awaitMessage("started_up");

  // Load AddonManager, and unload the extension as soon as it has started.
  await AddonTestUtils.promiseStartupManager();
  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();

  // Confirm that the extension has started when promiseStartupManager returned.
  info("Awaiting expected started_up message 2");
  await extension.awaitMessage("started_up");
});

// Check that the add-on from the previous test has indeed been uninstalled.
add_task(async function restart_addon_manager_after_extension_unload() {
  await AddonTestUtils.promiseStartupManager();
  let addon = await AddonManager.getAddonByID(TEST_ADDON_ID);
  equal(addon, null, "Test add-on should have been removed");
  await AddonTestUtils.promiseShutdownManager();
});
