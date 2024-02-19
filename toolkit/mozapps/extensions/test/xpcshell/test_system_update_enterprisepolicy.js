/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test verifies that system addon updates are correctly blocked by the
// DisableSystemAddonUpdate enterprise policy.

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

Services.policies; // Load policy engine

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"]);
distroDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
registerDirectory("XREAppFeat", distroDir);
add_task(() => initSystemAddonDirs());

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
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
  ],
};

add_task(async function test_update_disabled_by_policy() {
  await setupSystemAddonConditions(TEST_CONDITIONS, distroDir);

  const TEST_POLICY_DATA = {
    DisableSystemAddonUpdate: true,
  };
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: TEST_POLICY_DATA,
  });

  Assert.deepEqual(
    Services.policies.getActivePolicies(),
    TEST_POLICY_DATA,
    "Got the expected test policy data as the active policy " +
      "(if this assertions fails, check your system for enterprise policies installed at system level)"
  );

  Assert.equal(
    Services.policies.isAllowed("SysAddonUpdate"),
    false,
    "Expected SysAddonUpdate feature to be disabled by policies"
  );

  Assert.equal(
    Services.prefs.getBoolPref("extensions.systemAddon.update.enabled"),
    true,
    "Expected system addon updates to not be already disabled through prefs"
  );

  await updateAllSystemAddons(
    buildSystemAddonUpdates([
      {
        id: "system2@tests.mozilla.org",
        version: "2.0",
        path: "system2_2.xpi",
        xpi: await getSystemAddonXPI(2, "2.0"),
      },
      {
        id: "system3@tests.mozilla.org",
        version: "2.0",
        path: "system3_2.xpi",
        xpi: await getSystemAddonXPI(3, "2.0"),
      },
    ])
  );

  await verifySystemAddonState(
    TEST_CONDITIONS.initialState,
    undefined,
    false,
    distroDir
  );

  await promiseShutdownManager();
});
