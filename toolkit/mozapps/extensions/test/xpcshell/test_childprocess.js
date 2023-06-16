/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that the AddonManager refuses to load in child processes.

// NOTE: This test does NOT load head_addons.js, because that would indirectly
// load AddonManager.sys.mjs. In this test, we want to be the first to load the
// AddonManager module to verify that it cannot be loaded in child processes.

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

function run_test() {
  updateAppInfo();
  Services.appinfo.processType = Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
  try {
    ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");
    do_throw("AddonManager should have refused to load");
  } catch (ex) {
    info(ex.message);
    Assert.ok(!!ex.message);
  }
}
