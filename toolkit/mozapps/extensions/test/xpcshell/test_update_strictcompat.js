/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update checks work in conjunction with
// strict compatibility settings.

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

const appId = "toolkit@mozilla.org";

testserver = createHttpServer({ hosts: ["example.com"] });
testserver.registerDirectory("/data/", do_get_file("data"));

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  AddonTestUtils.updateReason = AddonManager.UPDATE_WHEN_USER_REQUESTED;

  Services.prefs.setCharPref(
    PREF_GETADDONS_BYIDS,
    "http://example.com/data/test_update_addons.json"
  );
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
});

// Test that the update check correctly observes the
// extensions.strictCompatibility pref.
add_task(async function test_update_strict() {
  const ID = "addon9@tests.mozilla.org";
  let xpi = await createAddon({
    id: ID,
    updateURL: "http://example.com/update.json",
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.1",
        maxVersion: "0.2",
      },
    ],
  });
  await manuallyInstall(xpi, AddonTestUtils.profileExtensions, ID);

  await promiseStartupManager();

  await AddonRepository.backgroundUpdateCheck();

  let UPDATE = {
    addons: {
      [ID]: {
        updates: [
          {
            version: "2.0",
            update_link: "http://example.com/addons/test_update9_2.xpi",
            applications: {
              gecko: {
                strict_min_version: "1",
                advisory_max_version: "1",
              },
            },
          },

          // Incompatible when strict compatibility is enabled
          {
            version: "3.0",
            update_link: "http://example.com/addons/test_update9_3.xpi",
            applications: {
              gecko: {
                strict_min_version: "0.9",
                advisory_max_version: "0.9",
              },
            },
          },

          // Addon for future version of app
          {
            version: "4.0",
            update_link: "http://example.com/addons/test_update9_5.xpi",
            applications: {
              gecko: {
                strict_min_version: "5",
                advisory_max_version: "6",
              },
            },
          },
        ],
      },
    },
  };

  AddonTestUtils.registerJSON(testserver, "/update.json", UPDATE);

  let addon = await AddonManager.getAddonByID(ID);
  let { updateAvailable } = await promiseFindAddonUpdates(addon);

  Assert.notEqual(updateAvailable, null, "Got update");
  Assert.equal(
    updateAvailable.version,
    "3.0",
    "The correct update was selected"
  );
  await addon.uninstall();

  await promiseShutdownManager();
});

// Tests that compatibility updates are applied to addons when the updated
// compatibility data wouldn't match with strict compatibility enabled.
add_task(async function test_update_strict2() {
  const ID = "addon10@tests.mozilla.org";
  let xpi = createAddon({
    id: ID,
    updateURL: "http://example.com/update.json",
    targetApplications: [
      {
        id: appId,
        minVersion: "0.1",
        maxVersion: "0.2",
      },
    ],
  });
  await manuallyInstall(xpi, AddonTestUtils.profileExtensions, ID);

  await promiseStartupManager();
  await AddonRepository.backgroundUpdateCheck();

  const UPDATE = {
    addons: {
      [ID]: {
        updates: [
          {
            version: "1.0",
            update_link: "http://example.com/addons/test_update10.xpi",
            applications: {
              gecko: {
                strict_min_version: "0.1",
                advisory_max_version: "0.4",
              },
            },
          },
        ],
      },
    },
  };

  AddonTestUtils.registerJSON(testserver, "/update.json", UPDATE);

  let addon = await AddonManager.getAddonByID(ID);
  notEqual(addon, null);

  let result = await promiseFindAddonUpdates(addon);
  ok(result.compatibilityUpdate, "Should have seen a compatibility update");
  ok(!result.updateAvailable, "Should not have seen a version update");

  await addon.uninstall();
  await promiseShutdownManager();
});

// Test that the update check correctly observes when an addon opts-in to
// strict compatibility checking.
add_task(async function test_update_strict_optin() {
  const ID = "addon11@tests.mozilla.org";
  let xpi = await createAddon({
    id: ID,
    updateURL: "http://example.com/update.json",
    targetApplications: [
      {
        id: appId,
        minVersion: "0.1",
        maxVersion: "0.2",
      },
    ],
  });
  await manuallyInstall(xpi, AddonTestUtils.profileExtensions, ID);

  await promiseStartupManager();

  await AddonRepository.backgroundUpdateCheck();

  const UPDATE = {
    addons: {
      [ID]: {
        updates: [
          {
            version: "2.0",
            update_link: "http://example.com/addons/test_update11.xpi",
            applications: {
              gecko: {
                strict_min_version: "0.1",
                strict_max_version: "0.2",
              },
            },
          },
        ],
      },
    },
  };

  AddonTestUtils.registerJSON(testserver, "/update.json", UPDATE);

  let addon = await AddonManager.getAddonByID(ID);
  notEqual(addon, null);

  let result = await AddonTestUtils.promiseFindAddonUpdates(addon);
  ok(
    !result.compatibilityUpdate,
    "Should not have seen a compatibility update"
  );
  ok(!result.updateAvailable, "Should not have seen a version update");

  await addon.uninstall();
  await promiseShutdownManager();
});
