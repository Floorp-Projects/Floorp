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
add_task(function*() {

  writeInstallRDFToDir({
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

  startupManager();

  let addon = yield promiseAddonByID(NORMAL_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test disabling hidden add-ons, non-hidden add-on case.");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_false(addon.userDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  // normal add-ons can be disabled by the user.
  addon.userDisabled = true;

  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test disabling hidden add-ons, non-hidden add-on case.");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.userDisabled);
  do_check_false(addon.isActive);
  do_check_eq(addon.type, "extension");

  addon.uninstall();

  shutdownManager();
});

// system add-ons can never be user disabled.
add_task(function*() {

  writeInstallRDFToDir({
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

  startupManager();

  let addon = yield promiseAddonByID(SYSTEM_ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test disabling hidden add-ons, hidden system add-on case.");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_false(addon.userDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  // system add-ons cannot be disabled by the user.
  try {
    addon.userDisabled = true;
    do_throw("Expected addon.userDisabled on a hidden add-on to throw!");
  } catch (e) {
    do_check_eq(e.message, `Cannot disable hidden add-on ${SYSTEM_ID}`);
  }

  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test disabling hidden add-ons, hidden system add-on case.");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_false(addon.userDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  shutdownManager();
});
