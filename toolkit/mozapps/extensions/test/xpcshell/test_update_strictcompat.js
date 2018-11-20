/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update checks work
// This file is a placeholder for now, it holds test cases related to
// strict compatibility to be moved or removed shortly.

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

const updateFile = "test_update.json";
const appId = "toolkit@mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

const ADDONS = {
  test_update: {
    id: "addon1@tests.mozilla.org",
    version: "2.0",
    name: "Test 1",
  },
  test_update8: {
    id: "addon8@tests.mozilla.org",
    version: "2.0",
    name: "Test 8",
  },
  test_update12: {
    id: "addon12@tests.mozilla.org",
    version: "2.0",
    name: "Test 12",
  },
  test_install2_1: {
    id: "addon2@tests.mozilla.org",
    version: "2.0",
    name: "Real Test 2",
  },
  test_install2_2: {
    id: "addon2@tests.mozilla.org",
    version: "3.0",
    name: "Real Test 3",
  },
};

var testserver = createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));

const XPIS = {};

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  Services.locale.requestedLocales = ["fr-FR"];

  for (let [name, info] of Object.entries(ADDONS)) {
    XPIS[name] = createTempWebExtensionFile({
      manifest: {
        name: info.name,
        version: info.version,
        applications: {gecko: {id: info.id}},
      },
    });
    testserver.registerFile(`/addons/${name}.xpi`, XPIS[name]);
  }

  AddonTestUtils.updateReason = AddonManager.UPDATE_WHEN_USER_REQUESTED;

  await promiseStartupManager();
});

// Test that the update check correctly observes the
// extensions.strictCompatibility pref and compatibility overrides.
add_task(async function test_17() {
  await promiseInstallWebExtension({
    manifest: {
      name: "Test Addon 9",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon9@tests.mozilla.org",
          update_url: "http://example.com/data/" + updateFile,
        },
      },
    },
  });

  let listener;
  await new Promise(resolve => {
    listener = {
      onNewInstall(aInstall) {
        equal(aInstall.existingAddon.id, "addon9@tests.mozilla.org",
              "Saw unexpected onNewInstall for " + aInstall.existingAddon.id);
        equal(aInstall.version, "3.0");
      },
      onDownloadFailed(aInstall) {
        resolve();
      },
    };
    AddonManager.addInstallListener(listener);

    Services.prefs.setCharPref(PREF_GETADDONS_BYIDS,
                               `http://example.com/data/test_update_addons.json`);
    Services.prefs.setCharPref(PREF_COMPAT_OVERRIDES,
                               `http://example.com/data/test_update_compat.json`);
    Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

    AddonManagerInternal.backgroundUpdateCheck();
  });

  AddonManager.removeInstallListener(listener);

  let a9 = await AddonManager.getAddonByID("addon9@tests.mozilla.org");
  await a9.uninstall();
});

// Tests that compatibility updates are applied to addons when the updated
// compatibility data wouldn't match with strict compatibility enabled.
add_task(async function test_18() {
  await promiseInstallXPI({
    id: "addon10@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    updateURL: "http://example.com/data/" + updateFile,
    targetApplications: [{
      id: appId,
      minVersion: "0.1",
      maxVersion: "0.2",
    }],
    name: "Test Addon 10",
  });

  let a10 = await AddonManager.getAddonByID("addon10@tests.mozilla.org");
  notEqual(a10, null);

  let result = await AddonTestUtils.promiseFindAddonUpdates(a10);
  ok(result.compatibilityUpdate, "Should have seen a compatibility update");
  ok(!result.updateAvailable, "Should not have seen a version update");

  await a10.uninstall();
});

// Test that the update check correctly observes when an addon opts-in to
// strict compatibility checking.
add_task(async function test_19() {
  await promiseInstallXPI({
    id: "addon11@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    updateURL: "http://example.com/data/" + updateFile,
    targetApplications: [{
      id: appId,
      minVersion: "0.1",
      maxVersion: "0.2",
    }],
    name: "Test Addon 11",
  });

  let a11 = await AddonManager.getAddonByID("addon11@tests.mozilla.org");
  notEqual(a11, null);

  let result = await AddonTestUtils.promiseFindAddonUpdates(a11);
  ok(!result.compatibilityUpdate, "Should not have seen a compatibility update");
  ok(!result.updateAvailable, "Should not have seen a version update");

  await a11.uninstall();
});

