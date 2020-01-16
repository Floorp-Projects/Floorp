/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Make Cu.isInAutomation true.
Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  true
);

// This verifies that app upgrades produce the expected behaviours,
// with strict compatibility checking disabled.

// turn on Cu.isInAutomation
Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);

Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

// Enable loading extensions from the application scope
Services.prefs.setIntPref(
  "extensions.enabledScopes",
  AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION
);
Services.prefs.setIntPref("extensions.sideloadScopes", AddonManager.SCOPE_ALL);

const profileDir = gProfD.clone();
profileDir.append("extensions");

const globalDir = Services.dirsvc.get("XREAddonAppDir", Ci.nsIFile);
globalDir.append("extensions");

var gGlobalExisted = globalDir.exists();
var gInstallTime = Date.now();

const ID1 = "addon1@tests.mozilla.org";
const ID2 = "addon2@tests.mozilla.org";
const ID3 = "addon3@tests.mozilla.org";
const ID4 = "addon4@tests.mozilla.org";
const PATH4 = OS.Path.join(globalDir.path, `${ID4}.xpi`);

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Will be compatible in the first version and incompatible in subsequent versions
  let xpi = await createAddon({
    id: ID1,
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1",
      },
    ],
    targetPlatforms: [{ os: "XPCShell" }, { os: "WINNT_x86" }],
  });
  await manuallyInstall(xpi, profileDir, ID1);

  // Works in all tested versions
  xpi = await createAddon({
    id: ID2,
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "2",
      },
    ],
    targetPlatforms: [
      {
        os: "XPCShell",
        abi: "noarch-spidermonkey",
      },
    ],
  });
  await manuallyInstall(xpi, profileDir, ID2);

  // Will be disabled in the first version and enabled in the second.
  xpi = createAddon({
    id: ID3,
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "2",
        maxVersion: "2",
      },
    ],
  });
  await manuallyInstall(xpi, profileDir, ID3);

  // Will be compatible in both versions but will change version in between
  xpi = await createAddon({
    id: ID4,
    version: "1.0",
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1",
      },
    ],
  });
  await manuallyInstall(xpi, globalDir, ID4);
  await promiseSetExtensionModifiedTime(PATH4, gInstallTime);
});

registerCleanupFunction(function end_test() {
  if (!gGlobalExisted) {
    globalDir.remove(true);
  } else {
    globalDir.append(do_get_expected_addon_name(ID4));
    globalDir.remove(true);
  }
});

// Test that the test extensions are all installed
add_task(async function test_1() {
  await promiseStartupManager();

  let [a1, a2, a3, a4] = await promiseAddonsByIDs([ID1, ID2, ID3, ID4]);
  Assert.notEqual(a1, null, "Found extension 1");
  Assert.equal(a1.isActive, true, "Extension 1 is active");

  Assert.notEqual(a2, null, "Found extension 2");
  Assert.equal(a2.isActive, true, "Extension 2 is active");

  Assert.notEqual(a3, null, "Found extension 3");
  Assert.equal(a3.isActive, false, "Extension 3 is not active");

  Assert.notEqual(a4, null);
  Assert.equal(a4.isActive, true);
  Assert.equal(a4.version, "1.0");
});

// Test that upgrading the application doesn't disable now incompatible add-ons
add_task(async function test_2() {
  await promiseShutdownManager();

  // Upgrade the extension
  let xpi = createAddon({
    id: ID4,
    version: "2.0",
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "2",
        maxVersion: "2",
      },
    ],
  });
  await manuallyInstall(xpi, globalDir, ID4);
  await promiseSetExtensionModifiedTime(PATH4, gInstallTime);

  await promiseStartupManager("2");
  let [a1, a2, a3, a4] = await promiseAddonsByIDs([ID1, ID2, ID3, ID4]);
  Assert.notEqual(a1, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(isExtensionInBootstrappedList(globalDir, a4.id));
  Assert.equal(a4.version, "2.0");
});

// Test that nothing changes when only the build ID changes.
add_task(async function test_3() {
  await promiseShutdownManager();

  // Upgrade the extension
  let xpi = createAddon({
    id: ID4,
    version: "3.0",
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "3",
        maxVersion: "3",
      },
    ],
  });
  await manuallyInstall(xpi, globalDir, ID4);
  await promiseSetExtensionModifiedTime(PATH4, gInstallTime);

  // Simulates a simple Build ID change
  gAddonStartup.remove(true);
  await promiseStartupManager();

  let [a1, a2, a3, a4] = await promiseAddonsByIDs([ID1, ID2, ID3, ID4]);

  Assert.notEqual(a1, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(isExtensionInBootstrappedList(globalDir, a4.id));
  Assert.equal(a4.version, "2.0");

  await promiseShutdownManager();
});
