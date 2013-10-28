/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that the AddonManager refuses to load in child processes.

function run_test() {
  // Already loaded the module by head_addons.js. Need to unload this again, so
  // that overriding the app-info and re-importing the module works.
  Components.utils.unload("resource://gre/modules/AddonManager.jsm");
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  gAppInfo.processType = AM_Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
  try {
    Components.utils.import("resource://gre/modules/AddonManager.jsm");
    do_throw("AddonManager should have refused to load");
  }
  catch (ex) {
    do_print(ex.message);
    do_check_true(!!ex.message);
  }
}
