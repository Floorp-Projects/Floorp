/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test verifies that hidden add-ons cannot be user disabled.

// for normal add-ons
const profileDir = FileUtils.getDir("ProfD", ["extensions"]);
// for system add-ons
const distroDir = FileUtils.getDir("ProfD", ["sysfeatures"], true);
registerDirectory("XREAppFeat", distroDir);

const NORMAL_ID = "normal@tests.mozilla.org";
const SYSTEM_ID = "system@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// normal add-ons can be user disabled.
add_task(async function() {

  await promiseWriteInstallRDFToXPI({
    id: NORMAL_ID,
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test disabling hidden add-ons, non-hidden add-on case.",
  }, profileDir, NORMAL_ID);

  await promiseStartupManager();

  let addon = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test disabling hidden add-ons, non-hidden add-on case.");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(!addon.userDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  // normal add-ons can be disabled by the user.
  addon.userDisabled = true;

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test disabling hidden add-ons, non-hidden add-on case.");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.userDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.type, "extension");

  addon.uninstall();

  await promiseShutdownManager();
});

// system add-ons can never be user disabled.
add_task(async function() {

  await promiseWriteInstallRDFToXPI({
    id: SYSTEM_ID,
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test disabling hidden add-ons, hidden system add-on case.",
  }, distroDir, SYSTEM_ID);

  await overrideBuiltIns({ "system": [SYSTEM_ID] });

  await promiseStartupManager();

  let addon = await promiseAddonByID(SYSTEM_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test disabling hidden add-ons, hidden system add-on case.");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(!addon.userDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  // system add-ons cannot be disabled by the user.
  try {
    addon.userDisabled = true;
    do_throw("Expected addon.userDisabled on a hidden add-on to throw!");
  } catch (e) {
    Assert.equal(e.message, `Cannot disable hidden add-on ${SYSTEM_ID}`);
  }

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test disabling hidden add-ons, hidden system add-on case.");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(!addon.userDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  await promiseShutdownManager();
});
