/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that delaying a system add-on update works.

ChromeUtils.import("resource://testing-common/httpd.js");
const profileDir = gProfD.clone();
profileDir.append("extensions");

const IGNORE_ID = "system_delay_ignore@tests.mozilla.org";
const COMPLETE_ID = "system_delay_complete@tests.mozilla.org";
const DEFER_ID = "system_delay_defer@tests.mozilla.org";
const DEFER_ALSO_ID = "system_delay_defer_also@tests.mozilla.org";
const NORMAL_ID = "system1@tests.mozilla.org";

const TEST_IGNORE_PREF = "delaytest.ignore";

const distroDir = FileUtils.getDir("ProfD", ["sysfeatures"], true);
registerDirectory("XREAppFeat", distroDir);

let testserver = new HttpServer();
testserver.registerDirectory("/data/", do_get_file("data/system_addons"));
testserver.start();
let root = `${testserver.identity.primaryScheme}://${testserver.identity.primaryHost}:${testserver.identity.primaryPort}/data/`;
Services.prefs.setCharPref(PREF_SYSTEM_ADDON_UPDATE_URL, root + "update.xml");


// Note that we would normally use BootstrapMonitor but it currently requires
// the objects in `data` to be serializable, and we need a real reference to the
// `instanceID` symbol to test.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

function promiseInstallPostponed(addonID1, addonID2) {
  return new Promise((resolve, reject) => {
    let seen = [];
    let listener = {
      onInstallFailed: () => {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded: (install) => {
        AddonManager.removeInstallListener(listener);
        reject(`extension installation should not have ended for ${install.addon.id}`);
      },
      onInstallPostponed: (install) => {
        seen.push(install.addon.id);
        if (seen.includes(addonID1) && seen.includes(addonID2)) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      }
    };

    AddonManager.addInstallListener(listener);
  });
}

function promiseInstallResumed(addonID1, addonID2) {
  return new Promise((resolve, reject) => {
    let seenPostponed = [];
    let seenEnded = [];
    let listener = {
      onInstallFailed: () => {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded: (install) => {
        seenEnded.push(install.addon.id);
        if ((seenEnded.includes(addonID1) && seenEnded.includes(addonID2)) &&
            (seenPostponed.includes(addonID1) && seenPostponed.includes(addonID2))) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      },
      onInstallPostponed: (install) => {
        seenPostponed.push(install.addon.id);
      }
    };

    AddonManager.addInstallListener(listener);
  });
}

function promiseInstallDeferred(addonID1, addonID2) {
  return new Promise((resolve, reject) => {
    let seenEnded = [];
    let listener = {
      onInstallFailed: () => {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded: (install) => {
        seenEnded.push(install.addon.id);
        if (seenEnded.includes(addonID1) && seenEnded.includes(addonID2)) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      },
    };

    AddonManager.addInstallListener(listener);
  });
}


// add-on registers upgrade listener, and ignores update.
add_task(async function() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  do_get_file("data/system_addons/system_delay_ignore.xpi").copyTo(distroDir, "system_delay_ignore@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system1_1.xpi").copyTo(distroDir, "system1@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": [IGNORE_ID, NORMAL_ID] });

  await promiseStartupManager();
  let updateList = [
    { id: IGNORE_ID, version: "2.0", path: "system_delay_ignore_2.xpi" },
    { id: NORMAL_ID, version: "2.0", path: "system1_2.xpi" },
  ];

  let postponed = promiseInstallPostponed(IGNORE_ID, NORMAL_ID);
  await installSystemAddons(await buildSystemAddonUpdates(updateList, root), testserver);
  await postponed;

  // addon upgrade has been delayed.
  let addon_postponed = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Test Delay Update Ignore");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");
  Assert.ok(Services.prefs.getBoolPref(TEST_IGNORE_PREF));

  // other addons in the set are delayed as well.
  addon_postponed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Add-on 1");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // restarting allows upgrades to proceed
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Test Delay Update Ignore");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Add-on 1");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

// add-on registers upgrade listener, and allows update.
add_task(async function() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  do_get_file("data/system_addons/system_delay_complete.xpi").copyTo(distroDir, "system_delay_complete@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system1_1.xpi").copyTo(distroDir, "system1@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": [COMPLETE_ID, NORMAL_ID] });

  await promiseStartupManager();

  let updateList = [
    { id: COMPLETE_ID, version: "2.0", path: "system_delay_complete_2.xpi" },
    { id: NORMAL_ID, version: "2.0", path: "system1_2.xpi" },
  ];

  // initial state
  let addon_allowed = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "1.0");
  Assert.equal(addon_allowed.name, "System Test Delay Update Complete");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  addon_allowed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "1.0");
  Assert.equal(addon_allowed.name, "System Add-on 1");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  let resumed = promiseInstallResumed(COMPLETE_ID, NORMAL_ID);
  await installSystemAddons(await buildSystemAddonUpdates(updateList, root), testserver);

  // update is initially postponed, then resumed
  await resumed;

  // addon upgrade has been allowed
  addon_allowed = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "System Test Delay Update Complete");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // other upgrades in the set are allowed as well
  addon_allowed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "System Add-on 1");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Test Delay Update Complete");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Add-on 1");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

// add-on registers upgrade listener, initially defers update then allows upgrade
add_task(async function() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  do_get_file("data/system_addons/system_delay_defer.xpi").copyTo(distroDir, "system_delay_defer@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system1_1.xpi").copyTo(distroDir, "system1@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": [DEFER_ID, NORMAL_ID] });

  await promiseStartupManager();

  let updateList = [
    { id: DEFER_ID, version: "2.0", path: "system_delay_defer_2.xpi" },
    { id: NORMAL_ID, version: "2.0", path: "system1_2.xpi" },
  ];

  let postponed = promiseInstallPostponed(DEFER_ID, NORMAL_ID);
  await installSystemAddons(await buildSystemAddonUpdates(updateList, root), testserver);
  await postponed;

  // upgrade is initially postponed
  let addon_postponed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Test Delay Update Defer");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // other addons in the set are postponed as well.
  addon_postponed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Add-on 1");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  let deferred = promiseInstallDeferred(DEFER_ID, NORMAL_ID);
  // add-on will not allow upgrade until fake event fires
  AddonManagerPrivate.callAddonListeners("onFakeEvent");

  await deferred;

  // addon upgrade has been allowed
  let addon_allowed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "System Test Delay Update Defer");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // other addons in the set are allowed as well.
  addon_allowed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "System Add-on 1");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Test Delay Update Defer");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Add-on 1");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

// multiple add-ons registers upgrade listeners, initially defers then each unblock in turn.
add_task(async function() {
  // discard system addon updates.
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  do_get_file("data/system_addons/system_delay_defer.xpi").copyTo(distroDir, "system_delay_defer@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system_delay_defer_also.xpi").copyTo(distroDir, "system_delay_defer_also@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": [DEFER_ID, DEFER_ALSO_ID] });

  await promiseStartupManager();

  let updateList = [
    { id: DEFER_ID, version: "2.0", path: "system_delay_defer_2.xpi" },
    { id: DEFER_ALSO_ID, version: "2.0", path: "system_delay_defer_also_2.xpi" },
  ];

  let postponed = promiseInstallPostponed(DEFER_ID, DEFER_ALSO_ID);
  await installSystemAddons(await buildSystemAddonUpdates(updateList, root), testserver);
  await postponed;

  // upgrade is initially postponed
  let addon_postponed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Test Delay Update Defer");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // other addons in the set are postponed as well.
  addon_postponed = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Test Delay Update Defer Also");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  let deferred = promiseInstallDeferred(DEFER_ID, DEFER_ALSO_ID);
  // add-on will not allow upgrade until fake event fires
  AddonManagerPrivate.callAddonListeners("onFakeEvent");

  // Upgrade blockers still present.
  addon_postponed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Test Delay Update Defer");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  addon_postponed = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.equal(addon_postponed.name, "System Test Delay Update Defer Also");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  AddonManagerPrivate.callAddonListeners("onOtherFakeEvent");

  await deferred;

  // addon upgrade has been allowed
  let addon_allowed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "System Test Delay Update Defer");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // other addons in the set are allowed as well.
  addon_allowed = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_allowed, null);
  // do_check_eq(addon_allowed.version, "2.0");
  Assert.equal(addon_allowed.name, "System Test Delay Update Defer Also");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Test Delay Update Defer");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.equal(addon_upgraded.name, "System Test Delay Update Defer Also");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});
