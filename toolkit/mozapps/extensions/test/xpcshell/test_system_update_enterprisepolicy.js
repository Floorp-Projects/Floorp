/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test verifies that system addon updates are correctly blocked by the
// DisableSystemAddonUpdate enterprise policy.

ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://testing-common/EnterprisePolicyTesting.jsm");

// Setting PREF_DISABLE_SECURITY tells the policy engine that we are in testing
// mode and enables restarting the policy engine without restarting the browser.
Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(PREF_DISABLE_SECURITY);
});

Services.policies; // Load policy engine

BootstrapMonitor.init();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

var testserver = new HttpServer();
testserver.registerDirectory("/data/", do_get_file("data/system_addons"));
testserver.start();
var root = testserver.identity.primaryScheme + "://" +
           testserver.identity.primaryHost + ":" +
           testserver.identity.primaryPort + "/data/";
Services.prefs.setCharPref(PREF_SYSTEM_ADDON_UPDATE_URL, root + "update.xml");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"], true);
registerDirectory("XREAppFeat", distroDir);
initSystemAddonDirs();

/**
 * Defines the set of initial conditions to run the test against.
 *
 * setup:        A task to setup the profile into the initial state.
 * initialState: The initial expected system add-on state after setup has run.
 *
 * These conditions run tests with no updated or default system add-ons
 * initially installed
 */
const TEST_CONDITIONS = {
  setup() {
    clearSystemAddonUpdatesDir();
    distroDir.leafName = "empty";
  },
  initialState: [
    { isUpgrade: false, version: null},
    { isUpgrade: false, version: null},
    { isUpgrade: false, version: null},
    { isUpgrade: false, version: null},
    { isUpgrade: false, version: null},
  ],
};

add_task(async function test_update_disabled_by_policy() {
  await setupSystemAddonConditions(TEST_CONDITIONS, distroDir);

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    "policies": {
      "DisableSystemAddonUpdate": true,
    },
  });

  await updateAllSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" },
  ], root), testserver);

  await verifySystemAddonState(TEST_CONDITIONS.initialState, undefined, false, distroDir);

  await promiseShutdownManager();
});
