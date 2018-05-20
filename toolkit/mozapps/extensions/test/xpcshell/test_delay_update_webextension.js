/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that delaying an update works for WebExtensions.

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

if (AppConstants.platform == "win" && AppConstants.DEBUG) {
  // Shutdown timing is flaky in this test, and remote extensions
  // sometimes wind up leaving the XPI locked at the point when we try
  // to remove it.
  Services.prefs.setBoolPref("extensions.webextensions.remote", false);
}

PromiseTestUtils.whitelistRejectionsGlobally(/Message manager disconnected/);

/* globals browser*/

const profileDir = gProfD.clone();
profileDir.append("extensions");
const stageDir = profileDir.clone();
stageDir.append("staged");

const IGNORE_ID = "test_delay_update_ignore_webext@tests.mozilla.org";
const COMPLETE_ID = "test_delay_update_complete_webext@tests.mozilla.org";
const DEFER_ID = "test_delay_update_defer_webext@tests.mozilla.org";
const NOUPDATE_ID = "test_no_update_webext@tests.mozilla.org";

// Create and configure the HTTP server.
var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));
testserver.registerDirectory("/addons/", do_get_file("addons"));

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

// add-on registers upgrade listener, and ignores update.
add_task(async function delay_updates_ignore() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": IGNORE_ID,
          "update_url": `http://example.com/data/test_delay_updates_ignore.json`,
        },
      },
    },
    background() {
      browser.runtime.onUpdateAvailable.addListener(details => {
        if (details) {
          if (details.version) {
            // This should be the version of the pending update.
            browser.test.assertEq("2.0", details.version, "correct version");
            browser.test.notifyPass("delay");
          }
        } else {
          browser.test.fail("no details object passed");
        }
      });
      browser.test.sendMessage("ready");
    },
  }, IGNORE_ID);

  await Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Generated extension");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  await promiseCompleteAllInstalls([install]);

  Assert.equal(install.state, AddonManager.STATE_POSTPONED);

  // addon upgrade has been delayed
  let addon_postponed = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "Generated extension");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  await extension.awaitFinish("delay");

  // restarting allows upgrade to proceed
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(IGNORE_ID);
  await extension.awaitStartup();

  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "Delay Upgrade");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await extension.unload();
  await promiseShutdownManager();
});

// add-on registers upgrade listener, and allows update.
add_task(async function delay_updates_complete() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": COMPLETE_ID,
          "update_url": `http://example.com/data/test_delay_updates_complete.json`,
        },
      },
    },
    background() {
      browser.runtime.onUpdateAvailable.addListener(details => {
        browser.test.notifyPass("reload");
        browser.runtime.reload();
      });
      browser.test.sendMessage("ready");
    },
  }, COMPLETE_ID);

  await Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Generated extension");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  let promiseInstalled = promiseAddonEvent("onInstalled");
  await promiseCompleteAllInstalls([install]);

  await extension.awaitFinish("reload");

  // addon upgrade has been allowed
  let [addon_allowed] = await promiseInstalled;
  await extension.awaitStartup();

  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "Delay Upgrade");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  if (stageDir.exists()) {
    do_throw("Staging directory should not exist for formerly-postponed extension");
  }

  await extension.unload();
  await promiseShutdownManager();
});

// add-on registers upgrade listener, initially defers update then allows upgrade
add_task(async function delay_updates_defer() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": DEFER_ID,
          "update_url": `http://example.com/data/test_delay_updates_defer.json`,
        },
      },
    },
    background() {
      browser.runtime.onUpdateAvailable.addListener(details => {
        // Upgrade will only proceed when "allow" message received.
        browser.test.onMessage.addListener(msg => {
          if (msg == "allow") {
            browser.test.notifyPass("allowed");
            browser.runtime.reload();
          } else {
            browser.test.fail(`wrong message: ${msg}`);
          }
        });
        browser.test.sendMessage("truly ready");
      });
      browser.test.sendMessage("ready");
    },
  }, DEFER_ID);

  await Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Generated extension");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  let promiseInstalled = promiseAddonEvent("onInstalled");
  await promiseCompleteAllInstalls([install]);

  Assert.equal(install.state, AddonManager.STATE_POSTPONED);

  // upgrade is initially postponed
  let addon_postponed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "Generated extension");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // add-on will not allow upgrade until message is received
  await extension.awaitMessage("truly ready");
  extension.sendMessage("allow");
  await extension.awaitFinish("allowed");

  // addon upgrade has been allowed
  let [addon_allowed] = await promiseInstalled;
  await extension.awaitStartup();

  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "Delay Upgrade");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  await promiseRestartManager();

  // restart changes nothing
  addon_allowed = await promiseAddonByID(DEFER_ID);
  await extension.awaitStartup();

  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "Delay Upgrade");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  await extension.unload();
  await promiseShutdownManager();
});

// browser.runtime.reload() without a pending upgrade should just reload.
add_task(async function runtime_reload() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": NOUPDATE_ID,
          "update_url": `http://example.com/data/test_no_update.json`,
        },
      },
    },
    background() {
      browser.test.onMessage.addListener(msg => {
        if (msg == "reload") {
          browser.runtime.reload();
        } else {
          browser.test.fail(`wrong message: ${msg}`);
        }
      });
      browser.test.sendMessage("ready");
    },
  }, NOUPDATE_ID);

  await Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = await promiseAddonByID(NOUPDATE_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Generated extension");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  await promiseFindAddonUpdates(addon);

  extension.sendMessage("reload");
  // Wait for extension to restart, to make sure reload works.
  await extension.awaitStartup();

  addon = await promiseAddonByID(NOUPDATE_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Generated extension");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  await extension.unload();
  await promiseShutdownManager();
});
