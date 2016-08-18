/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that we only check manifest age for disabled extensions

Components.utils.import("resource://gre/modules/Promise.jsm");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

/* We want one add-on installed packed, and one installed unpacked
 */

function run_test() {
  // Shut down the add-on manager after all tests run.
  do_register_cleanup(promiseShutdownManager);
  // Kick off the task-based tests...
  run_next_test();
}

// Use bootstrap extensions so the changes will be immediate.
// A packed extension, to be enabled
writeInstallRDFToXPI({
  id: "packed-enabled@tests.mozilla.org",
  version: "1.0",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }],
  name: "Packed, Enabled",
}, profileDir);

// Packed, will be disabled
writeInstallRDFToXPI({
  id: "packed-disabled@tests.mozilla.org",
  version: "1.0",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }],
  name: "Packed, Disabled",
}, profileDir);

// Unpacked, enabled
writeInstallRDFToDir({
  id: "unpacked-enabled@tests.mozilla.org",
  version: "1.0",
  bootstrap: true,
  unpack: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }],
  name: "Unpacked, Enabled",
}, profileDir, undefined, "extraFile.js");


// Unpacked, disabled
writeInstallRDFToDir({
  id: "unpacked-disabled@tests.mozilla.org",
  version: "1.0",
  bootstrap: true,
  unpack: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }],
  name: "Unpacked, disabled",
}, profileDir, undefined, "extraFile.js");

// Keep track of the last time stamp we've used, so that we can keep moving
// it forward (if we touch two different files in the same add-on with the same
// timestamp we may not consider the change significant)
var lastTimestamp = Date.now();

/*
 * Helper function to touch a file and then test whether we detect the change.
 * @param XS      The XPIState object.
 * @param aPath   File path to touch.
 * @param aChange True if we should notice the change, False if we shouldn't.
 */
function checkChange(XS, aPath, aChange) {
  do_check_true(aPath.exists());
  lastTimestamp += 10000;
  do_print("Touching file " + aPath.path + " with " + lastTimestamp);
  aPath.lastModifiedTime = lastTimestamp;
  do_check_eq(XS.getInstallState(), aChange);
  // Save the pref so we don't detect this change again
  XS.save();
}

// Get a reference to the XPIState (loaded by startupManager) so we can unit test it.
function getXS() {
  let XPI = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm");
  return XPI.XPIStates;
}

add_task(function* detect_touches() {
  startupManager();
  let [pe, pd, ue, ud] = yield promiseAddonsByIDs([
         "packed-enabled@tests.mozilla.org",
         "packed-disabled@tests.mozilla.org",
         "unpacked-enabled@tests.mozilla.org",
         "unpacked-disabled@tests.mozilla.org"
         ]);

  do_print("Disable test add-ons");
  pd.userDisabled = true;
  ud.userDisabled = true;

  let XS = getXS();

  // Should be no changes detected here, because everything should start out up-to-date.
  do_check_false(XS.getInstallState());

  let states = XS.getLocation("app-profile");

  // State should correctly reflect enabled/disabled
  do_check_true(states.get("packed-enabled@tests.mozilla.org").enabled);
  do_check_false(states.get("packed-disabled@tests.mozilla.org").enabled);
  do_check_true(states.get("unpacked-enabled@tests.mozilla.org").enabled);
  do_check_false(states.get("unpacked-disabled@tests.mozilla.org").enabled);

  // Touch various files and make sure the change is detected.

  // We notice that a packed XPI is touched for an enabled add-on.
  let peFile = profileDir.clone();
  peFile.append("packed-enabled@tests.mozilla.org.xpi");
  checkChange(XS, peFile, true);

  // We should notice the packed XPI change for a disabled add-on too.
  let pdFile = profileDir.clone();
  pdFile.append("packed-disabled@tests.mozilla.org.xpi");
  checkChange(XS, pdFile, true);

  // We notice changing install.rdf for an enabled unpacked add-on.
  let ueDir = profileDir.clone();
  ueDir.append("unpacked-enabled@tests.mozilla.org");
  let manifest = ueDir.clone();
  manifest.append("install.rdf");
  checkChange(XS, manifest, true);
  // We also notice changing another file for enabled unpacked add-on.
  let otherFile = ueDir.clone();
  otherFile.append("extraFile.js");
  checkChange(XS, otherFile, true);

  // We notice changing install.rdf for a *disabled* unpacked add-on.
  let udDir = profileDir.clone();
  udDir.append("unpacked-disabled@tests.mozilla.org");
  manifest = udDir.clone();
  manifest.append("install.rdf");
  checkChange(XS, manifest, true);
  // Finally, the case we actually care about...
  // We *don't* notice changing another file for disabled unpacked add-on.
  otherFile = udDir.clone();
  otherFile.append("extraFile.js");
  checkChange(XS, otherFile, false);

  /*
   * When we enable an unpacked add-on that was modified while it was
   * disabled, we reflect the new timestamp in the add-on DB (otherwise, we'll
   * think it changed on next restart).
   */
  ud.userDisabled = false;
  let xState = XS.getAddon("app-profile", ud.id);
  do_check_true(xState.enabled);
  do_check_eq(xState.scanTime, ud.updateDate.getTime());
});

/*
 * Uninstalling bootstrap add-ons should immediately remove them from the
 * extensions.xpiState preference.
 */
add_task(function* uninstall_bootstrap() {
  let [pe, pd, ue, ud] = yield promiseAddonsByIDs([
         "packed-enabled@tests.mozilla.org",
         "packed-disabled@tests.mozilla.org",
         "unpacked-enabled@tests.mozilla.org",
         "unpacked-disabled@tests.mozilla.org"
         ]);
  pe.uninstall();
  let xpiState = Services.prefs.getCharPref("extensions.xpiState");
  do_check_false(xpiState.includes("\"packed-enabled@tests.mozilla.org\""));
});

/*
 * Installing a restartless add-on should immediately add it to XPIState
 */
add_task(function* install_bootstrap() {
  let XS = getXS();

  let installer = yield new Promise((resolve, reject) =>
    AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), resolve));

  let promiseInstalled = new Promise((resolve, reject) => {
    AddonManager.addInstallListener({
      onInstallFailed: reject,
      onInstallEnded: (install, newAddon) => resolve(newAddon)
    });
  });

  installer.install();

  let newAddon = yield promiseInstalled;
  let xState = XS.getAddon("app-profile", newAddon.id);
  do_check_true(!!xState);
  do_check_true(xState.enabled);
  do_check_eq(xState.scanTime, newAddon.updateDate.getTime());
  newAddon.uninstall();
});

/*
 * Installing an add-on that requires restart doesn't add to XPIState
 * until after the restart; disable and enable happen immediately so that
 * the next restart won't / will scan as necessary on the next restart,
 * uninstalling it marks XPIState as disabled immediately
 * and removes XPIState after restart.
 */
add_task(function* install_restart() {
  let XS = getXS();

  let installer = yield new Promise((resolve, reject) =>
    AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_4"), resolve));

  let promiseInstalled = new Promise((resolve, reject) => {
    AddonManager.addInstallListener({
      onInstallFailed: reject,
      onInstallEnded: (install, newAddon) => resolve(newAddon)
    });
  });

  installer.install();

  let newAddon = yield promiseInstalled;
  let newID = newAddon.id;
  let xState = XS.getAddon("app-profile", newID);
  do_check_false(xState);

  // Now we restart the add-on manager, and we need to get the XPIState again
  // because the add-on manager reloads it.
  XS = null;
  newAddon = null;
  yield promiseRestartManager();
  XS = getXS();

  newAddon = yield promiseAddonByID(newID);
  xState = XS.getAddon("app-profile", newID);
  do_check_true(xState);
  do_check_true(xState.enabled);
  do_check_eq(xState.scanTime, newAddon.updateDate.getTime());

  // Check that XPIState enabled flag is updated immediately,
  // and doesn't change over restart.
  newAddon.userDisabled = true;
  do_check_false(xState.enabled);
  XS = null;
  newAddon = null;
  yield promiseRestartManager();
  XS = getXS();
  xState = XS.getAddon("app-profile", newID);
  do_check_true(xState);
  do_check_false(xState.enabled);

  newAddon = yield promiseAddonByID(newID);
  newAddon.userDisabled = false;
  do_check_true(xState.enabled);
  XS = null;
  newAddon = null;
  yield promiseRestartManager();
  XS = getXS();
  xState = XS.getAddon("app-profile", newID);
  do_check_true(xState);
  do_check_true(xState.enabled);

  // Uninstalling immediately marks XPIState disabled,
  // removes state after restart.
  newAddon = yield promiseAddonByID(newID);
  newAddon.uninstall();
  xState = XS.getAddon("app-profile", newID);
  do_check_true(xState);
  do_check_false(xState.enabled);

  // Restart to finish uninstall.
  XS = null;
  newAddon = null;
  yield promiseRestartManager();
  XS = getXS();
  xState = XS.getAddon("app-profile", newID);
  do_check_false(xState);
});
