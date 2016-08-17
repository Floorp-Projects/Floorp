/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that delaying an update works for WebExtensions.

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

/*globals browser*/

const profileDir = gProfD.clone();
profileDir.append("extensions");
const tempdir = gTmpD.clone();

const IGNORE_ID = "test_delay_update_ignore_webext@tests.mozilla.org";
const COMPLETE_ID = "test_delay_update_complete_webext@tests.mozilla.org";
const DEFER_ID = "test_delay_update_defer_webext@tests.mozilla.org";
const NOUPDATE_ID = "test_no_update_webext@tests.mozilla.org";

// Create and configure the HTTP server.
let testserver = createHttpServer();
gPort = testserver.identity.primaryPort;
mapFile("/data/test_delay_updates_complete.json", testserver);
mapFile("/data/test_delay_updates_ignore.json", testserver);
mapFile("/data/test_delay_updates_defer.json", testserver);
mapFile("/data/test_no_update.json", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const { Management } = Components.utils.import("resource://gre/modules/Extension.jsm", {});

function promiseWebExtensionStartup() {
  return new Promise(resolve => {
    let listener = (extension) => {
      Management.off("startup", listener);
      resolve(extension);
    };

    Management.on("startup", listener);
  });
}

// add-on registers upgrade listener, and ignores update.
add_task(function* delay_updates_ignore() {
  startupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": IGNORE_ID,
          "update_url": `http://localhost:${gPort}/data/test_delay_updates_ignore.json`,
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

  yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = yield promiseAddonByID(IGNORE_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Generated extension");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  let update = yield promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  yield promiseCompleteAllInstalls([install]);

  do_check_eq(install.state, AddonManager.STATE_POSTPONED);

  // addon upgrade has been delayed
  let addon_postponed = yield promiseAddonByID(IGNORE_ID);
  do_check_neq(addon_postponed, null);
  do_check_eq(addon_postponed.version, "1.0");
  do_check_eq(addon_postponed.name, "Generated extension");
  do_check_true(addon_postponed.isCompatible);
  do_check_false(addon_postponed.appDisabled);
  do_check_true(addon_postponed.isActive);
  do_check_eq(addon_postponed.type, "extension");

  yield extension.awaitFinish("delay");

  // restarting allows upgrade to proceed
  yield extension.markUnloaded();
  yield promiseRestartManager();

  let addon_upgraded = yield promiseAddonByID(IGNORE_ID);
  yield promiseWebExtensionStartup();

  do_check_neq(addon_upgraded, null);
  do_check_eq(addon_upgraded.version, "2.0");
  do_check_eq(addon_upgraded.name, "Delay Upgrade");
  do_check_true(addon_upgraded.isCompatible);
  do_check_false(addon_upgraded.appDisabled);
  do_check_true(addon_upgraded.isActive);
  do_check_eq(addon_upgraded.type, "extension");

  yield addon_upgraded.uninstall();
  yield promiseShutdownManager();
});

// add-on registers upgrade listener, and allows update.
add_task(function* delay_updates_complete() {
  startupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": COMPLETE_ID,
          "update_url": `http://localhost:${gPort}/data/test_delay_updates_complete.json`,
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

  yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = yield promiseAddonByID(COMPLETE_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Generated extension");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  let update = yield promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  let promiseInstalled = promiseAddonEvent("onInstalled");
  yield promiseCompleteAllInstalls([install]);

  yield extension.awaitFinish("reload");

  // addon upgrade has been allowed
  let [addon_allowed] = yield promiseInstalled;
  yield promiseWebExtensionStartup();

  do_check_neq(addon_allowed, null);
  do_check_eq(addon_allowed.version, "2.0");
  do_check_eq(addon_allowed.name, "Delay Upgrade");
  do_check_true(addon_allowed.isCompatible);
  do_check_false(addon_allowed.appDisabled);
  do_check_true(addon_allowed.isActive);
  do_check_eq(addon_allowed.type, "extension");

  yield extension.markUnloaded();
  yield addon_allowed.uninstall();
  yield promiseShutdownManager();
});

// add-on registers upgrade listener, initially defers update then allows upgrade
add_task(function* delay_updates_defer() {
  startupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": DEFER_ID,
          "update_url": `http://localhost:${gPort}/data/test_delay_updates_defer.json`,
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
      });
      browser.test.sendMessage("ready");
    },
  }, DEFER_ID);

  yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = yield promiseAddonByID(DEFER_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Generated extension");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  let update = yield promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  let promiseInstalled = promiseAddonEvent("onInstalled");
  yield promiseCompleteAllInstalls([install]);

  do_check_eq(install.state, AddonManager.STATE_POSTPONED);

  // upgrade is initially postponed
  let addon_postponed = yield promiseAddonByID(DEFER_ID);
  do_check_neq(addon_postponed, null);
  do_check_eq(addon_postponed.version, "1.0");
  do_check_eq(addon_postponed.name, "Generated extension");
  do_check_true(addon_postponed.isCompatible);
  do_check_false(addon_postponed.appDisabled);
  do_check_true(addon_postponed.isActive);
  do_check_eq(addon_postponed.type, "extension");

  // add-on will not allow upgrade until message is received
  extension.sendMessage("allow");
  yield extension.awaitFinish("allowed");

  // addon upgrade has been allowed
  let [addon_allowed] = yield promiseInstalled;
  yield promiseWebExtensionStartup();

  do_check_neq(addon_allowed, null);
  do_check_eq(addon_allowed.version, "2.0");
  do_check_eq(addon_allowed.name, "Delay Upgrade");
  do_check_true(addon_allowed.isCompatible);
  do_check_false(addon_allowed.appDisabled);
  do_check_true(addon_allowed.isActive);
  do_check_eq(addon_allowed.type, "extension");

  yield extension.markUnloaded();
  yield promiseRestartManager();

  // restart changes nothing
  addon_allowed = yield promiseAddonByID(DEFER_ID);
  yield promiseWebExtensionStartup();

  do_check_neq(addon_allowed, null);
  do_check_eq(addon_allowed.version, "2.0");
  do_check_eq(addon_allowed.name, "Delay Upgrade");
  do_check_true(addon_allowed.isCompatible);
  do_check_false(addon_allowed.appDisabled);
  do_check_true(addon_allowed.isActive);
  do_check_eq(addon_allowed.type, "extension");

  yield addon_allowed.uninstall();
  yield promiseShutdownManager();
});

// browser.runtime.reload() without a pending upgrade should just reload.
add_task(function* runtime_reload() {
  startupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": NOUPDATE_ID,
          "update_url": `http://localhost:${gPort}/data/test_no_update.json`,
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

  yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let addon = yield promiseAddonByID(NOUPDATE_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Generated extension");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  yield promiseFindAddonUpdates(addon);

  extension.sendMessage("reload");
  // Wait for extension to restart, to make sure reload works.
  yield promiseWebExtensionStartup();

  addon = yield promiseAddonByID(NOUPDATE_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Generated extension");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  yield extension.markUnloaded();
  yield addon.uninstall();
  yield promiseShutdownManager();
});
