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


const ADDONS = {
  test_delay_update_complete_v2: {
    "install.rdf": {
      "id": "test_delay_update_complete@tests.mozilla.org",
      "version": "2.0",
      "name": "Test Delay Update Complete",
    },
  },
  test_delay_update_defer_v2: {
    "install.rdf": {
      "id": "test_delay_update_defer@tests.mozilla.org",
      "version": "2.0",
      "name": "Test Delay Update Defer",
    },
  },
  test_delay_update_ignore_v2: {
    "install.rdf": {
      "id": "test_delay_update_ignore@tests.mozilla.org",
      "version": "2.0",
      "name": "Test Delay Update Ignore",
    },
  },
};

const XPIS = {};
for (let [name, files] of Object.entries(ADDONS)) {
  XPIS[name] = AddonTestUtils.createTempXPIFile(files);
  testserver.registerFile(`/addons/${name}.xpi`, XPIS[name]);
}

async function createIgnoreAddon() {
  await promiseWriteInstallRDFToXPI({
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
  }, profileDir, IGNORE_ID, {
    "bootstrap.js": String.raw`
      ChromeUtils.import("resource://gre/modules/Services.jsm");
      ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

      const ADDON_ID = "test_delay_update_ignore@tests.mozilla.org";
      const TEST_IGNORE_PREF = "delaytest.ignore";

      function install(data, reason) {}

      // normally we would use BootstrapMonitor here, but we need a reference to
      // the symbol inside XPIProvider.jsm.
      function startup(data, reason) {
        Services.prefs.setBoolPref(TEST_IGNORE_PREF, false);

        // explicitly ignore update, will be queued for next restart
        if (data.hasOwnProperty("instanceID") && data.instanceID) {
          AddonManager.addUpgradeListener(data.instanceID, (upgrade) => {
            Services.prefs.setBoolPref(TEST_IGNORE_PREF, true);
          });
        } else {
          throw Error("no instanceID passed to bootstrap startup");
        }
      }

      function shutdown(data, reason) {}

      function uninstall(data, reason) {}
    `,
  });
}

async function createCompleteAddon() {
  await promiseWriteInstallRDFToXPI({
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
  }, profileDir, COMPLETE_ID, {
    "bootstrap.js": String.raw`
      ChromeUtils.import("resource://gre/modules/Services.jsm");
      ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

      const ADDON_ID = "test_delay_update_complete@tests.mozilla.org";
      const INSTALL_COMPLETE_PREF = "bootstraptest.install_complete_done";

      function install(data, reason) {}

      // normally we would use BootstrapMonitor here, but we need a reference to
      // the symbol inside XPIProvider.jsm.
      function startup(data, reason) {
        // apply update immediately
        if (data.hasOwnProperty("instanceID") && data.instanceID) {
          AddonManager.addUpgradeListener(data.instanceID, (upgrade) => {
            upgrade.install();
          });
        } else {
          throw Error("no instanceID passed to bootstrap startup");
        }
      }

      function shutdown(data, reason) {}

      function uninstall(data, reason) {}
    `,
  });
}

async function createDeferAddon() {
  await promiseWriteInstallRDFToXPI({
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
  }, profileDir, DEFER_ID, {
    "bootstrap.js": String.raw`
      ChromeUtils.import("resource://gre/modules/Services.jsm");
      ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

      const ADDON_ID = "test_delay_update_complete@tests.mozilla.org";
      const INSTALL_COMPLETE_PREF = "bootstraptest.install_complete_done";

      // global reference to hold upgrade object
      let gUpgrade;

      function install(data, reason) {}

      // normally we would use BootstrapMonitor here, but we need a reference to
      // the symbol inside XPIProvider.jsm.
      function startup(data, reason) {
        // do not apply update immediately, hold on to for later
        if (data.hasOwnProperty("instanceID") && data.instanceID) {
          AddonManager.addUpgradeListener(data.instanceID, (upgrade) => {
            gUpgrade = upgrade;
          });
        } else {
          throw Error("no instanceID passed to bootstrap startup");
        }

        // add a listener so the test can pass control back
        AddonManager.addAddonListener({
          onFakeEvent: () => {
            gUpgrade.install();
          }
        });
      }

      function shutdown(data, reason) {}

      function uninstall(data, reason) {}
    `,
  });
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
