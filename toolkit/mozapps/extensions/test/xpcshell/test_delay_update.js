/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that delaying an update works

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

Components.utils.import("resource://testing-common/httpd.js");
const profileDir = gProfD.clone();
profileDir.append("extensions");
const tempdir = gTmpD.clone();

const IGNORE_ID = "test_delay_update_ignore@tests.mozilla.org";
const COMPLETE_ID = "test_delay_update_complete@tests.mozilla.org";
const DEFER_ID = "test_delay_update_defer@tests.mozilla.org";

const TEST_IGNORE_PREF = "delaytest.ignore";

// Note that we would normally use BootstrapMonitor but it currently requires
// the objects in `data` to be serializable, and we need a real reference to the
// `instanceID` symbol to test.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// Create and configure the HTTP server.
let testserver = createHttpServer();
gPort = testserver.identity.primaryPort;
mapFile("/data/test_delay_updates_complete.rdf", testserver);
mapFile("/data/test_delay_updates_ignore.rdf", testserver);
mapFile("/data/test_delay_updates_defer.rdf", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

function* createIgnoreAddon() {
  writeInstallRDFToDir({
    id: IGNORE_ID,
    version: "1.0",
    bootstrap: true,
    unpack: true,
    updateURL: `http://localhost:${gPort}/data/test_delay_updates_ignore.rdf`,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Delay Update Ignore",
  }, profileDir, IGNORE_ID, "bootstrap.js");

  let unpacked_addon = profileDir.clone();
  unpacked_addon.append(IGNORE_ID);
  do_get_file("data/test_delay_update_ignore/bootstrap.js")
    .copyTo(unpacked_addon, "bootstrap.js");
}

function* createCompleteAddon() {
  writeInstallRDFToDir({
    id: COMPLETE_ID,
    version: "1.0",
    bootstrap: true,
    unpack: true,
    updateURL: `http://localhost:${gPort}/data/test_delay_updates_complete.rdf`,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Delay Update Complete",
  }, profileDir, COMPLETE_ID, "bootstrap.js");

  let unpacked_addon = profileDir.clone();
  unpacked_addon.append(COMPLETE_ID);
  do_get_file("data/test_delay_update_complete/bootstrap.js")
    .copyTo(unpacked_addon, "bootstrap.js");
}

function* createDeferAddon() {
  writeInstallRDFToDir({
    id: DEFER_ID,
    version: "1.0",
    bootstrap: true,
    unpack: true,
    updateURL: `http://localhost:${gPort}/data/test_delay_updates_defer.rdf`,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Delay Update Defer",
  }, profileDir, DEFER_ID, "bootstrap.js");

  let unpacked_addon = profileDir.clone();
  unpacked_addon.append(DEFER_ID);
  do_get_file("data/test_delay_update_defer/bootstrap.js")
    .copyTo(unpacked_addon, "bootstrap.js");
}

// add-on registers upgrade listener, and ignores update.
add_task(function*() {

  yield createIgnoreAddon();

  startupManager();

  let addon = yield promiseAddonByID(IGNORE_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Delay Update Ignore");
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
  do_check_eq(addon_postponed.name, "Test Delay Update Ignore");
  do_check_true(addon_postponed.isCompatible);
  do_check_false(addon_postponed.appDisabled);
  do_check_true(addon_postponed.isActive);
  do_check_eq(addon_postponed.type, "extension");
  do_check_true(Services.prefs.getBoolPref(TEST_IGNORE_PREF));

  // restarting allows upgrade to proceed
  yield promiseRestartManager();

  let addon_upgraded = yield promiseAddonByID(IGNORE_ID);
  do_check_neq(addon_upgraded, null);
  do_check_eq(addon_upgraded.version, "2.0");
  do_check_eq(addon_upgraded.name, "Test Delay Update Ignore");
  do_check_true(addon_upgraded.isCompatible);
  do_check_false(addon_upgraded.appDisabled);
  do_check_true(addon_upgraded.isActive);
  do_check_eq(addon_upgraded.type, "extension");

  yield shutdownManager();
});

// add-on registers upgrade listener, and allows update.
add_task(function*() {

  yield createCompleteAddon();

  startupManager();

  let addon = yield promiseAddonByID(COMPLETE_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Delay Update Complete");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  let update = yield promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  yield promiseCompleteAllInstalls([install]);

  // upgrade is initially postponed
  let addon_postponed = yield promiseAddonByID(COMPLETE_ID);
  do_check_neq(addon_postponed, null);
  do_check_eq(addon_postponed.version, "1.0");
  do_check_eq(addon_postponed.name, "Test Delay Update Complete");
  do_check_true(addon_postponed.isCompatible);
  do_check_false(addon_postponed.appDisabled);
  do_check_true(addon_postponed.isActive);
  do_check_eq(addon_postponed.type, "extension");

  // addon upgrade has been allowed
  let [addon_allowed] = yield promiseAddonEvent("onInstalled");
  do_check_neq(addon_allowed, null);
  do_check_eq(addon_allowed.version, "2.0");
  do_check_eq(addon_allowed.name, "Test Delay Update Complete");
  do_check_true(addon_allowed.isCompatible);
  do_check_false(addon_allowed.appDisabled);
  do_check_true(addon_allowed.isActive);
  do_check_eq(addon_allowed.type, "extension");

  // restarting changes nothing
  yield promiseRestartManager();

  let addon_upgraded = yield promiseAddonByID(COMPLETE_ID);
  do_check_neq(addon_upgraded, null);
  do_check_eq(addon_upgraded.version, "2.0");
  do_check_eq(addon_upgraded.name, "Test Delay Update Complete");
  do_check_true(addon_upgraded.isCompatible);
  do_check_false(addon_upgraded.appDisabled);
  do_check_true(addon_upgraded.isActive);
  do_check_eq(addon_upgraded.type, "extension");

  yield shutdownManager();
});

// add-on registers upgrade listener, initially defers update then allows upgrade
add_task(function*() {

  yield createDeferAddon();

  startupManager();

  let addon = yield promiseAddonByID(DEFER_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Delay Update Defer");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  let update = yield promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  yield promiseCompleteAllInstalls([install]);

  // upgrade is initially postponed
  let addon_postponed = yield promiseAddonByID(DEFER_ID);
  do_check_neq(addon_postponed, null);
  do_check_eq(addon_postponed.version, "1.0");
  do_check_eq(addon_postponed.name, "Test Delay Update Defer");
  do_check_true(addon_postponed.isCompatible);
  do_check_false(addon_postponed.appDisabled);
  do_check_true(addon_postponed.isActive);
  do_check_eq(addon_postponed.type, "extension");

  // add-on will not allow upgrade until fake event fires
  AddonManagerPrivate.callAddonListeners("onFakeEvent");

  // addon upgrade has been allowed
  let [addon_allowed] = yield promiseAddonEvent("onInstalled");
  do_check_neq(addon_allowed, null);
  do_check_eq(addon_allowed.version, "2.0");
  do_check_eq(addon_allowed.name, "Test Delay Update Defer");
  do_check_true(addon_allowed.isCompatible);
  do_check_false(addon_allowed.appDisabled);
  do_check_true(addon_allowed.isActive);
  do_check_eq(addon_allowed.type, "extension");

  // restarting changes nothing
  yield promiseRestartManager();

  let addon_upgraded = yield promiseAddonByID(DEFER_ID);
  do_check_neq(addon_upgraded, null);
  do_check_eq(addon_upgraded.version, "2.0");
  do_check_eq(addon_upgraded.name, "Test Delay Update Defer");
  do_check_true(addon_upgraded.isCompatible);
  do_check_false(addon_upgraded.appDisabled);
  do_check_true(addon_upgraded.isActive);
  do_check_eq(addon_upgraded.type, "extension");

  yield shutdownManager();
});
