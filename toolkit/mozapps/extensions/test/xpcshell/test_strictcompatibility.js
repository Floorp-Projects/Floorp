/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests AddonManager.strictCompatibility and it's related preference,
// extensions.strictCompatibility, and the strictCompatibility option in
// install.rdf

// turn on Cu.isInAutomation
Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);

// The `compatbile` array defines which of the tests below the add-on
// should be compatible in. It's pretty gross.
const ADDONS = [
  // Always compatible
  {
    manifest: {
      id: "addon1@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: {
      nonStrict: true,
      strict: true,
    },
  },

  // Incompatible in strict compatibility mode
  {
    manifest: {
      id: "addon2@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "0.7",
          maxVersion: "0.8",
        },
      ],
    },
    compatible: {
      nonStrict: true,
      strict: false,
    },
  },

  // Opt-in to strict compatibility - always incompatible
  {
    manifest: {
      id: "addon3@tests.mozilla.org",
      strictCompatibility: true,
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "0.8",
          maxVersion: "0.9",
        },
      ],
    },
    compatible: {
      nonStrict: false,
      strict: false,
    },
  },

  // Addon from the future - would be marked as compatibile-by-default,
  // but minVersion is higher than the app version
  {
    manifest: {
      id: "addon4@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "3",
          maxVersion: "5",
        },
      ],
    },
    compatible: {
      nonStrict: false,
      strict: false,
    },
  },

  // Dictionary - compatible even in strict compatibility mode
  {
    manifest: {
      id: "addon5@tests.mozilla.org",
      type: "dictionary",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "0.8",
          maxVersion: "0.9",
        },
      ],
    },
    compatible: {
      nonStrict: true,
      strict: true,
    },
  },
];

async function checkCompatStatus(strict, index) {
  info(`Checking compat status for test ${index}\n`);

  equal(AddonManager.strictCompatibility, strict);

  for (let test of ADDONS) {
    let { id } = test.manifest;
    let addon = await promiseAddonByID(id);
    checkAddon(id, addon, {
      isCompatible: test.compatible[index],
      appDisabled: !test.compatible[index],
    });
  }
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  for (let addon of ADDONS) {
    let xpi = await createAddon(addon.manifest);
    await manuallyInstall(
      xpi,
      AddonTestUtils.profileExtensions,
      addon.manifest.id
    );
  }

  await promiseStartupManager();
});

add_task(async function test_1() {
  info("Test 1");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  await checkCompatStatus(false, "nonStrict");
  await promiseRestartManager();
  await checkCompatStatus(false, "nonStrict");
});

add_task(async function test_2() {
  info("Test 2");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  await checkCompatStatus(true, "strict");
  await promiseRestartManager();
  await checkCompatStatus(true, "strict");
});
