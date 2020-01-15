/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// turn on Cu.isInAutomation
Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);

const ID = "addon1@tests.mozilla.org";
add_task(async function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  let xpi = createAddon({
    id: ID,
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.1",
        maxVersion: "0.2",
      },
    ],
  });
  await manuallyInstall(xpi, AddonTestUtils.profileExtensions, ID);

  AddonManager.strictCompatibility = false;
  await promiseStartupManager();

  let addon = await AddonManager.getAddonByID(ID);
  Assert.notEqual(addon, null);
  await addon.disable();

  Assert.ok(addon.userDisabled);
  Assert.ok(!addon.isActive);
  Assert.ok(!addon.appDisabled);

  let promise = promiseAddonEvent("onPropertyChanged");
  AddonManager.strictCompatibility = true;
  let [, properties] = await promise;

  Assert.deepEqual(
    properties,
    ["appDisabled"],
    "Got onPropertyChanged for appDisabled"
  );
  Assert.ok(addon.appDisabled);

  promise = promiseAddonEvent("onPropertyChanged");
  AddonManager.strictCompatibility = false;
  [, properties] = await promise;

  Assert.deepEqual(
    properties,
    ["appDisabled"],
    "Got onPropertyChanged for appDisabled"
  );
  Assert.ok(!addon.appDisabled);
});
