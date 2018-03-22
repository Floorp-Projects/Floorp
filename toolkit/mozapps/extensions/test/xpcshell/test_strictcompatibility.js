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
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: [true,  true,  true,  true],
  },

  // Incompatible in strict compatibility mode
  "addon2@tests.mozilla.org": {
    "install.rdf": {
      id: "addon2@tests.mozilla.org",
      version: "1.0",
      name: "Test 2",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.7",
        maxVersion: "0.8"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: [false, true,  false, true],
  },

  // Theme - always uses strict compatibility, so is always incompatible
  "addon3@tests.mozilla.org": {
    "install.rdf": {
      id: "addon3@tests.mozilla.org",
      version: "1.0",
      name: "Test 3",
      internalName: "test-theme-3",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.8",
        maxVersion: "0.9"
      }]
    },
    expected: {
      strictCompatibility: true,
    },
    compatible: [false, false, false, false],
  },

  // Opt-in to strict compatibility - always incompatible
  "addon4@tests.mozilla.org": {
    "install.rdf": {
      id: "addon4@tests.mozilla.org",
      version: "1.0",
      name: "Test 4",
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
    compatible: [false, false, false, false],
  },

  // Addon from the future - would be marked as compatibile-by-default,
  // but minVersion is higher than the app version
  "addon5@tests.mozilla.org": {
    "install.rdf": {
      id: "addon5@tests.mozilla.org",
      version: "1.0",
      name: "Test 5",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "3",
        maxVersion: "5"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: [false, false, false, false],
  },

  // Extremely old addon - maxVersion is less than the mimimum compat version
  // set in extensions.minCompatibleVersion
  "addon6@tests.mozilla.org": {
    "install.rdf": {
      id: "addon6@tests.mozilla.org",
      version: "1.0",
      name: "Test 6",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.1",
        maxVersion: "0.2"
      }]
    },
    expected: {
      strictCompatibility: false,
    },
    compatible: [false, true,  false, false],
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
    compatible: [false, true,  false, true],
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
    writeInstallRDFForExtension(addon["install.rdf"], profileDir);
  }

  Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0.1");

  await promiseStartupManager();
});

add_task(async function test_0() {
  // Should default to enabling strict compat.
  await checkCompatStatus(true, 0);
});

add_task(async function test_1() {
  info("Test 1");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  await checkCompatStatus(false, 1);
  await promiseRestartManager();
  await checkCompatStatus(false, 1);
});

add_task(async function test_2() {
  info("Test 2");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  await checkCompatStatus(true, 2);
  await promiseRestartManager();
  await checkCompatStatus(true, 2);
});

add_task(async function test_3() {
  info("Test 3");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0.4");
  await checkCompatStatus(false, 3);
});
