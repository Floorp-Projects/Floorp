/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const PASS_PREF = "symboltest.instanceid.pass";
const FAIL_BOGUS_PREF = "symboltest.instanceid.fail_bogus";
const FAIL_ID_PREF = "symboltest.instanceid.fail_bogus";
const ADDON_ID = "test_symbol@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

BootstrapMonitor.init();

// symbol is passed when add-on is installed
add_task(async function() {
  PromiseTestUtils.expectUncaughtRejection(/no addon found for symbol/);

  for (let pref of [PASS_PREF, FAIL_BOGUS_PREF, FAIL_ID_PREF])
    Services.prefs.clearUserPref(pref);

  await promiseInstallAllFiles([do_get_addon("test_symbol")], true);

  let addon = await promiseAddonByID(ADDON_ID);

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Symbol");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  // most of the test is in bootstrap.js in the addon because BootstrapMonitor
  // currently requires the objects in `data` to be serializable, and we
  // need a real reference to the symbol to test this.
  executeSoon(function() {
    // give the startup time to run
    Assert.ok(Services.prefs.getBoolPref(PASS_PREF));
    Assert.ok(Services.prefs.getBoolPref(FAIL_BOGUS_PREF));
    Assert.ok(Services.prefs.getBoolPref(FAIL_ID_PREF));
  });

  await promiseRestartManager();
});
