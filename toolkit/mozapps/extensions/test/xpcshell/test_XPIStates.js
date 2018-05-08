/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that we only check manifest age for disabled extensions


createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

/* We want one add-on installed packed, and one installed unpacked
 */

function run_test() {
  // Shut down the add-on manager after all tests run.
  registerCleanupFunction(promiseShutdownManager);
  // Kick off the task-based tests...
  run_next_test();
}

// Use bootstrap extensions so the changes will be immediate.
// A packed extension, to be enabled
add_task(async function setup() {
  await promiseWriteInstallRDFToXPI({
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
  await promiseWriteInstallRDFToXPI({
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
  await promiseWriteInstallRDFToDir({
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
  await promiseWriteInstallRDFToDir({
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
});

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
  Assert.ok(aPath.exists());
  lastTimestamp += 10000;
  info("Touching file " + aPath.path + " with " + lastTimestamp);
  aPath.lastModifiedTime = lastTimestamp;
  Assert.equal(XS.getInstallState(), aChange);
  // Save the pref so we don't detect this change again
  XS.save();
}

// Get a reference to the XPIState (loaded by startupManager) so we can unit test it.
function getXS() {
  let XPI = ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
  return XPI.XPIStates;
}

async function getXSJSON() {
  await AddonTestUtils.loadAddonsList(true);

  return aomStartup.readStartupData();
}

add_task(async function detect_touches() {
  await promiseStartupManager();
  let [/* pe */, pd, /* ue */, ud] = await promiseAddonsByIDs([
         "packed-enabled@tests.mozilla.org",
         "packed-disabled@tests.mozilla.org",
         "unpacked-enabled@tests.mozilla.org",
         "unpacked-disabled@tests.mozilla.org"
         ]);

  info("Disable test add-ons");
  pd.userDisabled = true;
  ud.userDisabled = true;

  let XS = getXS();

  // Should be no changes detected here, because everything should start out up-to-date.
  Assert.ok(!XS.getInstallState());

  let states = XS.getLocation("app-profile");

  // State should correctly reflect enabled/disabled
  Assert.ok(states.get("packed-enabled@tests.mozilla.org").enabled);
  Assert.ok(!states.get("packed-disabled@tests.mozilla.org").enabled);
  Assert.ok(states.get("unpacked-enabled@tests.mozilla.org").enabled);
  Assert.ok(!states.get("unpacked-disabled@tests.mozilla.org").enabled);

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

  // We notice changing install.rdf for a *disabled* unpacked add-on.
  let udDir = profileDir.clone();
  udDir.append("unpacked-disabled@tests.mozilla.org");
  manifest = udDir.clone();
  manifest.append("install.rdf");
  checkChange(XS, manifest, true);
  // Finally, the case we actually care about...
  // We *don't* notice changing another file for disabled unpacked add-on.
  let otherFile = udDir.clone();
  otherFile.append("extraFile.js");
  checkChange(XS, otherFile, false);

  /*
   * When we enable an unpacked add-on that was modified while it was
   * disabled, we reflect the new timestamp in the add-on DB (otherwise, we'll
   * think it changed on next restart).
   */
  ud.userDisabled = false;
  let xState = XS.getAddon("app-profile", ud.id);
  Assert.ok(xState.enabled);
  Assert.equal(xState.mtime, ud.updateDate.getTime());
});

/*
 * Uninstalling bootstrap add-ons should immediately remove them from the
 * extensions.xpiState preference.
 */
add_task(async function uninstall_bootstrap() {
  let [pe, /* pd, ue, ud */] = await promiseAddonsByIDs([
         "packed-enabled@tests.mozilla.org",
         "packed-disabled@tests.mozilla.org",
         "unpacked-enabled@tests.mozilla.org",
         "unpacked-disabled@tests.mozilla.org"
         ]);
  pe.uninstall();

  let xpiState = await getXSJSON();
  Assert.equal(false, "packed-enabled@tests.mozilla.org" in xpiState["app-profile"].addons);
});

/*
 * Installing a restartless add-on should immediately add it to XPIState
 */
add_task(async function install_bootstrap() {
  let XS = getXS();

  let installer = await promiseInstallFile(
    do_get_addon("test_bootstrap1_1"));

  let newAddon = installer.addon;
  let xState = XS.getAddon("app-profile", newAddon.id);
  Assert.ok(!!xState);
  Assert.ok(xState.enabled);
  Assert.equal(xState.mtime, newAddon.updateDate.getTime());
  newAddon.uninstall();
});
