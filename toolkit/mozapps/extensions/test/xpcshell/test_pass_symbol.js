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
add_task(function*() {
  for (let pref of [PASS_PREF, FAIL_BOGUS_PREF, FAIL_ID_PREF])
    Services.prefs.clearUserPref(pref);

  yield promiseInstallAllFiles([do_get_addon("test_symbol")], true);

  let addon = yield promiseAddonByID(ADDON_ID);

  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Symbol");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");

  // most of the test is in bootstrap.js in the addon because BootstrapMonitor
  // currently requires the objects in `data` to be serializable, and we
  // need a real reference to the symbol to test this.
  do_execute_soon(function() {
    // give the startup time to run
    do_check_true(Services.prefs.getBoolPref(PASS_PREF));
    do_check_true(Services.prefs.getBoolPref(FAIL_BOGUS_PREF));
    do_check_true(Services.prefs.getBoolPref(FAIL_ID_PREF));
  });

  yield promiseRestartManager();
});
