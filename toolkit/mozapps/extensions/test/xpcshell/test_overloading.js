/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that "overloading" an add-on in a higher-precedence
// install location works.
const PREF_SYSTEM_ADDON_SET = "extensions.systemAddonSet";
const PREF_UPDATE_SECURITY = "extensions.checkUpdateSecurity";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_UPDATE_SECURITY, false);

Components.utils.import("resource://testing-common/httpd.js");
const profileDir = gProfD.clone();
profileDir.append("extensions");
const tempdir = gTmpD.clone();

const OVERLOADED_ID = "test_overload@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// Create and configure the HTTP server.
let testserver = createHttpServer();
gPort = testserver.identity.primaryPort;
mapFile("/data/test_overload.json", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

// this is the "built-in" system add-on location
const distroDir = FileUtils.getDir("ProfD", ["sysfeatures"], true);
// this is the "updated" system add-on location
const updatesDir = FileUtils.getDir("ProfD", ["features", "set1"], true);
const tempDir = gTmpD.clone();

// precedence is: distroDir < updatesDir < profile
registerDirectory("XREAppFeat", distroDir);

BootstrapMonitor.init();

// bootstrap.js to inject into XPI files.
const mockBootstrapFile = {
  "bootstrap.js": `Components.utils.import("resource://xpcshell-data/BootstrapMonitor.jsm").monitor(this);`
}


// Override a system add-on with a:
// 1. system add-on update
// 2. regular (profile) add-on
//
// Then remove each in reverse order and ensure the correct add-on is revealed.
add_task(function*() {

  writeInstallRDFToDir({
    id: OVERLOADED_ID,
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Overload, system add-on default",
  }, distroDir, OVERLOADED_ID);

  startupManager();

  const systemAddon = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(systemAddon, null);
  do_check_eq(systemAddon.version, "1.0");
  do_check_eq(systemAddon.name, "Overload, system add-on default");
  do_check_true(systemAddon.isCompatible);
  do_check_false(systemAddon.appDisabled);
  do_check_true(systemAddon.isActive);
  do_check_eq(systemAddon.type, "extension");

  // write a system add-on upgrade to the upgrade dir
  writeInstallRDFToXPI({
    id: OVERLOADED_ID,
    version: "2.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Overload, system add-on upgrade",
  }, updatesDir, OVERLOADED_ID);

  // Set the system addon pref to the expected value.
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "test_overload@tests.mozilla.org": {
        version: "2.0"
      }
    }
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  // system add-on upgrades currently require restart (bug 1204156).
  yield promiseRestartManager();

  const systemAddonUpgrade = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(systemAddonUpgrade, null);
  do_check_eq(systemAddonUpgrade.version, "2.0");
  do_check_eq(systemAddonUpgrade.name, "Overload, system add-on upgrade");
  do_check_true(systemAddonUpgrade.isCompatible);
  do_check_false(systemAddonUpgrade.appDisabled);
  do_check_true(systemAddonUpgrade.isActive);
  do_check_eq(systemAddonUpgrade.type, "extension");

  // install new add-on with same ID "on top" of lower-precedence location.
  let normalXpiFile = writeInstallRDFToXPI({
    id: OVERLOADED_ID,
    version: "3.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Overload, normal",
  }, tempDir, OVERLOADED_ID, mockBootstrapFile);

  yield promiseInstallAllFiles([normalXpiFile]);

  const overloadedAddon = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(overloadedAddon, null);
  do_check_eq(overloadedAddon.version, "3.0");
  do_check_eq(overloadedAddon.name, "Overload, normal");
  do_check_true(overloadedAddon.isCompatible);
  do_check_false(overloadedAddon.appDisabled);
  do_check_true(overloadedAddon.isActive);
  do_check_eq(overloadedAddon.type, "extension");

  BootstrapMonitor.checkAddonStarted(OVERLOADED_ID, "3.0");

  // disabling does not reveal underlying add-on, "top" stays disabled
  overloadedAddon.userDisabled = true;
  do_check_false(overloadedAddon.isActive);
  do_check_eq(overloadedAddon.version, "3.0");

  overloadedAddon.userDisabled = false;
  do_check_true(overloadedAddon.isActive);
  do_check_eq(overloadedAddon.version, "3.0");

  yield overloadedAddon.uninstall();
  yield normalXpiFile.remove(false);

  // the "system add-on upgrade" is now revealed
  const originalSystemAddonUpgrade = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(originalSystemAddonUpgrade, null);
  do_check_eq(originalSystemAddonUpgrade.version, "2.0");
  do_check_eq(originalSystemAddonUpgrade.name, "Overload, system add-on upgrade");
  do_check_true(originalSystemAddonUpgrade.isCompatible);
  do_check_false(originalSystemAddonUpgrade.appDisabled);
  do_check_true(originalSystemAddonUpgrade.isActive);
  do_check_eq(originalSystemAddonUpgrade.type, "extension");

  // system add-on upgrades currently require restart (bug 1204156).
  yield updatesDir.remove(true);
  yield promiseRestartManager();

  do_check_false(updatesDir.exists());

  // the "system add-on default" is now revealed
  const originalSystemAddon = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(originalSystemAddon, null);
  do_check_eq(originalSystemAddon.version, "1.0");
  do_check_eq(originalSystemAddon.name, "Overload, system add-on default");
  do_check_true(originalSystemAddon.isCompatible);
  do_check_false(originalSystemAddon.appDisabled);
  do_check_true(originalSystemAddon.isActive);
  do_check_eq(originalSystemAddon.type, "extension");

  yield shutdownManager();
});

//  test that updates when overriding locked locations works.
add_task(function*() {
  writeInstallRDFToDir({
    id: OVERLOADED_ID,
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Overload, system add-on default",
  }, distroDir, OVERLOADED_ID);

  startupManager();

  const systemAddon = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(systemAddon, null);
  do_check_eq(systemAddon.version, "1.0");
  do_check_eq(systemAddon.name, "Overload, system add-on default");
  do_check_true(systemAddon.isCompatible);
  do_check_false(systemAddon.appDisabled);
  do_check_true(systemAddon.isActive);
  do_check_eq(systemAddon.type, "extension");

  // install new add-on with same ID "on top" of lower-precedence location.
  let normalXpiFile = writeInstallRDFToXPI({
    id: OVERLOADED_ID,
    version: "2.0",
    bootstrap: true,
    updateURL: `http://localhost:${gPort}/data/test_overload.json`,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Overload, normal",
  }, tempDir, OVERLOADED_ID, mockBootstrapFile);

  yield promiseInstallAllFiles([normalXpiFile]);

  const overloadedAddon = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(overloadedAddon, null);
  do_check_eq(overloadedAddon.version, "2.0");
  do_check_eq(overloadedAddon.name, "Overload, normal");
  do_check_true(overloadedAddon.isCompatible);
  do_check_false(overloadedAddon.appDisabled);
  do_check_true(overloadedAddon.isActive);
  do_check_eq(overloadedAddon.type, "extension");

  BootstrapMonitor.checkAddonStarted(OVERLOADED_ID, "2.0");

  // the "normal" add-on should allow updates, even though the underlying
  // add-on is in a locked location which does not.
  let update = yield promiseFindAddonUpdates(overloadedAddon);
  let install = update.updateAvailable;
  yield promiseCompleteAllInstalls([install]);

  const upgradedAddon = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(upgradedAddon, null);
  do_check_eq(upgradedAddon.version, "3.0");
  do_check_eq(upgradedAddon.name, "Overload, normal, upgraded");
  do_check_true(upgradedAddon.isCompatible);
  do_check_false(upgradedAddon.appDisabled);
  do_check_true(upgradedAddon.isActive);
  do_check_eq(upgradedAddon.type, "extension");

  yield upgradedAddon.uninstall();
  yield normalXpiFile.remove(false);

  // the system add-on default is now revealed.
  const originalSystemAddon = yield promiseAddonByID(OVERLOADED_ID);
  do_check_neq(originalSystemAddon, null);
  do_check_eq(originalSystemAddon.version, "1.0");
  do_check_eq(originalSystemAddon.name, "Overload, system add-on default");
  do_check_true(originalSystemAddon.isCompatible);
  do_check_false(originalSystemAddon.appDisabled);
  do_check_true(originalSystemAddon.isActive);
  do_check_eq(originalSystemAddon.type, "extension");

  yield shutdownManager();
});
