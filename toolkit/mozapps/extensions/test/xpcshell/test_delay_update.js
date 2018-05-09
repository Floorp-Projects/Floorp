/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that delaying an update works

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const profileDir = gProfD.clone();
profileDir.append("extensions");

const IGNORE_ID = "test_delay_update_ignore@tests.mozilla.org";
const COMPLETE_ID = "test_delay_update_complete@tests.mozilla.org";
const DEFER_ID = "test_delay_update_defer@tests.mozilla.org";

const TEST_IGNORE_PREF = "delaytest.ignore";

// Note that we would normally use BootstrapMonitor but it currently requires
// the objects in `data` to be serializable, and we need a real reference to the
// `instanceID` symbol to test.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// Create and configure the HTTP server.
var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));
testserver.registerDirectory("/addons/", do_get_file("addons"));

async function createIgnoreAddon() {
  await promiseWriteInstallRDFToDir({
    id: IGNORE_ID,
    version: "1.0",
    bootstrap: true,
    unpack: true,
    updateURL: `http://example.com/data/test_delay_updates_ignore_legacy.json`,
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

async function createCompleteAddon() {
  await promiseWriteInstallRDFToDir({
    id: COMPLETE_ID,
    version: "1.0",
    bootstrap: true,
    unpack: true,
    updateURL: `http://example.com/data/test_delay_updates_complete_legacy.json`,
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

async function createDeferAddon() {
  await promiseWriteInstallRDFToDir({
    id: DEFER_ID,
    version: "1.0",
    bootstrap: true,
    unpack: true,
    updateURL: `http://example.com/data/test_delay_updates_defer_legacy.json`,
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
add_task(async function() {

  await createIgnoreAddon();

  await promiseStartupManager();

  let addon = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Delay Update Ignore");
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
  Assert.equal(addon_postponed.name, "Test Delay Update Ignore");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");
  Assert.ok(Services.prefs.getBoolPref(TEST_IGNORE_PREF));

  // restarting allows upgrade to proceed
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "Test Delay Update Ignore");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

// add-on registers upgrade listener, and allows update.
add_task(async function() {

  await createCompleteAddon();

  await promiseStartupManager();

  let addon = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Delay Update Complete");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  await promiseCompleteAllInstalls([install]);

  // upgrade is initially postponed
  let addon_postponed = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "Test Delay Update Complete");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // addon upgrade has been allowed
  let [addon_allowed] = await promiseAddonEvent("onInstalled");
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "Test Delay Update Complete");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "Test Delay Update Complete");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

// add-on registers upgrade listener, initially defers update then allows upgrade
add_task(async function() {

  await createDeferAddon();

  await promiseStartupManager();

  let addon = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Delay Update Defer");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  await promiseCompleteAllInstalls([install]);

  // upgrade is initially postponed
  let addon_postponed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "Test Delay Update Defer");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // add-on will not allow upgrade until fake event fires
  AddonManagerPrivate.callAddonListeners("onFakeEvent");

  // addon upgrade has been allowed
  let [addon_allowed] = await promiseAddonEvent("onInstalled");
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "Test Delay Update Defer");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "Test Delay Update Defer");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});
