/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that we only check manifest age for disabled extensions

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

add_task(async function setup() {
  await promiseStartupManager();
  registerCleanupFunction(promiseShutdownManager);

  await promiseInstallWebExtension({
    manifest: {
      applications: { gecko: { id: "enabled@tests.mozilla.org" } },
    },
  });
  await promiseInstallWebExtension({
    manifest: {
      applications: { gecko: { id: "disabled@tests.mozilla.org" } },
    },
  });

  let addon = await promiseAddonByID("disabled@tests.mozilla.org");
  notEqual(addon, null);
  await addon.disable();
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
  Assert.equal(XS.scanForChanges(), aChange);
  // Save the pref so we don't detect this change again
  XS.save();
}

// Get a reference to the XPIState (loaded by startupManager) so we can unit test it.
function getXS() {
  let XPI = ChromeUtils.import(
    "resource://gre/modules/addons/XPIProvider.jsm",
    null
  );
  return XPI.XPIStates;
}

async function getXSJSON() {
  await AddonTestUtils.loadAddonsList(true);

  return aomStartup.readStartupData();
}

add_task(async function detect_touches() {
  let XS = getXS();

  // Should be no changes detected here, because everything should start out up-to-date.
  Assert.ok(!XS.scanForChanges());

  let states = XS.getLocation("app-profile");

  // State should correctly reflect enabled/disabled

  let state = states.get("enabled@tests.mozilla.org");
  Assert.notEqual(state, null, "Found xpi state for enabled extension");
  Assert.ok(state.enabled, "enabled extension has correct xpi state");

  state = states.get("disabled@tests.mozilla.org");
  Assert.notEqual(state, null, "Found xpi state for disabled extension");
  Assert.ok(!state.enabled, "disabled extension has correct xpi state");

  // Touch various files and make sure the change is detected.

  // We notice that a packed XPI is touched for an enabled add-on.
  let peFile = profileDir.clone();
  peFile.append("enabled@tests.mozilla.org.xpi");
  checkChange(XS, peFile, true);

  // We should notice the packed XPI change for a disabled add-on too.
  let pdFile = profileDir.clone();
  pdFile.append("disabled@tests.mozilla.org.xpi");
  checkChange(XS, pdFile, true);
});

/*
 * Uninstalling extensions should immediately remove them from XPIStates.
 */
add_task(async function uninstall_bootstrap() {
  let pe = await promiseAddonByID("enabled@tests.mozilla.org");
  await pe.uninstall();

  let xpiState = await getXSJSON();
  Assert.equal(
    false,
    "enabled@tests.mozilla.org" in xpiState["app-profile"].addons
  );
});

/*
 * Installing an extension should immediately add it to XPIState
 */
add_task(async function install_bootstrap() {
  const ID = "addon@tests.mozilla.org";
  let XS = getXS();

  await promiseInstallWebExtension({
    manifest: {
      applications: { gecko: { id: ID } },
    },
  });
  let addon = await promiseAddonByID(ID);

  let xState = XS.getAddon("app-profile", ID);
  Assert.ok(!!xState);
  Assert.ok(xState.enabled);
  Assert.equal(xState.mtime, addon.updateDate.getTime());
  await addon.uninstall();
});
