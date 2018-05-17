/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

/* globals Preferences */
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

function getXS() {
  let XPI = ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
  return XPI.XPIStates;
}

function installExtension(id, data) {
  return AddonTestUtils.promiseWriteFilesToExtension(
    AddonTestUtils.profileExtensions.path, id, data);
}

add_task(async function test_migrate_prefs() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "54");

  ok(!AddonTestUtils.addonStartup.exists(),
     "addonStartup.json.lz4 should not exist");

  const ID1 = "bootstrapped-enabled@xpcshell.mozilla.org";
  const ID2 = "bootstrapped-disabled@xpcshell.mozilla.org";

  let targetApplications = [{ id: "toolkit@mozilla.org", "minVersion": "0", "maxVersion": "*" }];

  let file1 = await installExtension(ID1, { "install.rdf": { id: ID1, name: ID1, bootstrap: true, version: "0.1", targetApplications } });
  let file2 = await installExtension(ID2, { "install.rdf": { id: ID2, name: ID2, bootstrap: true, version: "0.2", targetApplications } });

  function mt(file) {
    let f = file.clone();
    if (TEST_UNPACKED) {
      f.append("install.rdf");
    }
    return f.lastModifiedTime;
  }

  // Startup and shut down the add-on manager so the add-ons are added
  // to the DB.
  await promiseStartupManager();
  await promiseShutdownManager();

  // Remove the startup state file and add legacy prefs to replace it.
  AddonTestUtils.addonStartup.remove(false);

  Preferences.set("extensions.xpiState", JSON.stringify({
    "app-profile": {
      [ID1]: {e: true, d: file1.persistentDescriptor, v: "0.1", mt: mt(file1)},
      [ID2]: {e: false, d: file2.persistentDescriptor, v: "0.2", mt: mt(file2)},
    }
  }));

  Preferences.set("extensions.bootstrappedAddons", JSON.stringify({
    [ID1]: {
      version: "0.1",
      type: "extension",
      descriptor: file1.persistentDescriptor,
      hasEmbeddedWebExtension: true,
    }
  }));

  await promiseStartupManager();

  // Check the the state data is updated correctly.
  let states = getXS();

  let addon1 = states.findAddon(ID1);
  ok(addon1.enabled, "Addon 1 should be enabled");
  equal(addon1.version, "0.1", "Addon 1 has the correct version");
  equal(addon1.mtime, mt(file1), "Addon 1 has the correct timestamp");
  ok(addon1.hasEmbeddedWebExtension, "Addon 1 has an embedded WebExtension");

  let addon2 = states.findAddon(ID2);
  ok(!addon2.enabled, "Addon 2 should not be enabled");
  equal(addon2.version, "0.2", "Addon 2 has the correct version");
  equal(addon2.mtime, mt(file2), "Addon 2 has the correct timestamp");
  ok(!addon2.hasEmbeddedWebExtension, "Addon 2 no embedded WebExtension");

  // Check that legacy prefs and files have been removed.
  ok(!Preferences.has("extensions.xpiState"), "No xpiState pref left behind");
  ok(!Preferences.has("extensions.bootstrappedAddons"), "No bootstrappedAddons pref left behind");
  ok(!Preferences.has("extensions.enabledAddons"), "No enabledAddons pref left behind");

  let file = AddonTestUtils.profileDir.clone();
  file.append("extensions.ini");
  ok(!file.exists(), "No extensions.ini file left behind");

  await promiseShutdownManager();
});
