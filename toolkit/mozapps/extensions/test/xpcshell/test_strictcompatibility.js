/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests AddonManager.strictCompatibility and it's related preference,
// extensions.strictCompatibility, and the strictCompatibility option in
// install.rdf


// The `compatbile` array defines which of the tests below the add-on
// should be compatible in. It's pretty gross.
const ADDONS = {
  // Always compatible
  "addon1@tests.mozilla.org": {
    "install.rdf": {
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      name: "Test 1",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: {
      nonStrict: true,
      strict: true,
    },
  },

  // Incompatible in strict compatibility mode
  "addon2@tests.mozilla.org": {
    "install.rdf": {
      id: "addon2@tests.mozilla.org",
      version: "1.0",
      name: "Test 2",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.7",
        maxVersion: "0.8"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: {
      nonStrict: true,
      strict: false,
    },
  },

  // Opt-in to strict compatibility - always incompatible
  "addon4@tests.mozilla.org": {
    "install.rdf": {
      id: "addon4@tests.mozilla.org",
      version: "1.0",
      name: "Test 4",
      bootstrap: true,
      strictCompatibility: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.8",
        maxVersion: "0.9"
      }]
    },
    expected: {
      strictCompatibility: true,
    },
    compatible: {
      nonStrict: false,
      strict: false,
    },
  },

  // Addon from the future - would be marked as compatibile-by-default,
  // but minVersion is higher than the app version
  "addon5@tests.mozilla.org": {
    "install.rdf": {
      id: "addon5@tests.mozilla.org",
      version: "1.0",
      name: "Test 5",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "3",
        maxVersion: "5"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: {
      nonStrict: false,
      strict: false,
    },
  },

  // Extremely old addon - maxVersion is less than the minimum compat version
  // set in extensions.minCompatibleVersion
  "addon6@tests.mozilla.org": {
    "install.rdf": {
      id: "addon6@tests.mozilla.org",
      version: "1.0",
      name: "Test 6",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.1",
        maxVersion: "0.2"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: {
      nonStrict: true,
      strict: false,
    },
  },

  // Dictionary - incompatible in strict compatibility mode
  "addon7@tests.mozilla.org": {
    "install.rdf": {
      id: "addon7@tests.mozilla.org",
      version: "1.0",
      name: "Test 7",
      type: "64",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.8",
        maxVersion: "0.9"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: {
      nonStrict: true,
      strict: false,
    },
  },
};

const IDS = Object.keys(ADDONS);

const profileDir = gProfD.clone();
profileDir.append("extensions");

async function checkCompatStatus(strict, index) {
  info(`Checking compat status for test ${index}\n`);

  equal(AddonManager.strictCompatibility, strict);

  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id), {
      ...addon.expected,
      isCompatible: addon.compatible[index],
      appDisabled: !addon.compatible[index],
    });
  }
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  for (let addon of Object.values(ADDONS)) {
    await promiseWriteInstallRDFForExtension(addon["install.rdf"], profileDir);
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

const CHECK_COMPAT_ADDONS = {
  "cc-addon1@tests.mozilla.org": {
    "install.rdf": {
      // Cannot be enabled as it has no target app info for the applciation
      id: "cc-addon1@tests.mozilla.org",
      version: "1.0",
      name: "Test 1",
      bootstrap: true,
      targetApplications: [{
        id: "unknown@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },
    compatible: false,
    canOverride: false,
  },

  "cc-addon2@tests.mozilla.org": {
    "install.rdf": {
      // Always appears incompatible but can be enabled if compatibility checking is
      // disabled
      id: "cc-addon2@tests.mozilla.org",
      version: "1.0",
      name: "Test 2",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },
    compatible: false,
    canOverride: true,
  },

  "cc-addon4@tests.mozilla.org": {
    "install.rdf": {
      // Always compatible and enabled
      id: "cc-addon4@tests.mozilla.org",
      version: "1.0",
      name: "Test 4",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1",
        maxVersion: "2"
      }]
    },
    compatible: true,
  },

  "cc-addon5@tests.mozilla.org": {
    "install.rdf": {
      // Always compatible and enabled
      id: "cc-addon5@tests.mozilla.org",
      version: "1.0",
      name: "Test 5",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "3"
      }]
    },
    compatible: true,
  },
};

const CHECK_COMPAT_IDS = Object.keys(CHECK_COMPAT_ADDONS);

async function checkCompatOverrides(overridden) {
  let addons = await getAddons(CHECK_COMPAT_IDS);

  for (let [id, addon] of Object.entries(CHECK_COMPAT_ADDONS)) {
    checkAddon(id, addons.get(id), {
      isCompatible: addon.compatible,
      isActive: addon.compatible || (overridden && addon.canOverride),
    });
  }
}

var gIsNightly;

add_task(async function setupCheckCompat() {
  gIsNightly = isNightlyChannel();

  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);

  Object.assign(AddonTestUtils.appInfo,
                {version: "2.2.3", platformVersion: "2"});

  for (let addon of Object.values(CHECK_COMPAT_ADDONS)) {
    await promiseWriteInstallRDFForExtension(addon["install.rdf"], profileDir);
  }
  await promiseRestartManager("2.2.3");
});

// Tests that with compatibility checking enabled we see the incompatible
// add-ons disabled
add_task(async function test_compat_overrides_1() {
  await checkCompatOverrides(false);
});

// Tests that with compatibility checking disabled we see the incompatible
// add-ons enabled
add_task(async function test_compat_overrides_2() {
  if (gIsNightly)
    Services.prefs.setBoolPref("extensions.checkCompatibility.nightly", false);
  else
    Services.prefs.setBoolPref("extensions.checkCompatibility.2.2", false);

  await promiseRestartManager();

  await checkCompatOverrides(true);
});

// Tests that with compatibility checking disabled we see the incompatible
// add-ons enabled.
add_task(async function test_compat_overrides_3() {
  if (!gIsNightly)
    Services.prefs.setBoolPref("extensions.checkCompatibility.2.1a", false);
  await promiseRestartManager("2.1a4");

  await checkCompatOverrides(true);
});

// Tests that with compatibility checking enabled we see the incompatible
// add-ons disabled.
add_task(async function test_compat_overrides_4() {
  if (gIsNightly)
    Services.prefs.setBoolPref("extensions.checkCompatibility.nightly", true);
  else
    Services.prefs.setBoolPref("extensions.checkCompatibility.2.1a", true);
  await promiseRestartManager();

  await checkCompatOverrides(false);
});
