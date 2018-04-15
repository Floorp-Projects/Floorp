/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests compatibility overrides, for when strict compatibility checking is
// disabled. See bug 693906.


const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

ChromeUtils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);
gPort = gServer.identity.primaryPort;

const PORT            = gPort;
const BASE_URL        = "http://localhost:" + PORT;

const GETADDONS_RESPONSE = {
  next: null,
  previous: null,
  results: [],
};
gServer.registerPathHandler("/addons.json", (request, response) => {
  response.setHeader("content-type", "application/json");
  response.write(JSON.stringify(GETADDONS_RESPONSE));
});

const COMPAT_RESPONSE = {
  next: null,
  previous: null,
  results: [
    {
      addon_guid: "addon3@tests.mozilla.org",
      version_ranges: [
        {
          addon_min_version: "0.9",
          addon_max_version: "1.0",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "1",
              max_version: "2",
            },
          ],
        }
      ]
    },
    {
      addon_guid: "addon4@tests.mozilla.org",
      version_ranges: [
        {
          addon_min_version: "0.9",
          addon_max_version: "1.0",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "1",
              max_version: "2",
            },
          ],
        },
      ],
    },
    {
      addon_guid: "addon5@tests.mozilla.org",
      version_ranges: [
        {
          addon_min_version: "0.9",
          addon_max_version: "1.0",
          applications: [
            {
              name: "Unknown App",
              guid: "unknown-app@tests.mozilla.org",
              min_version: "1",
              max_version: "2",
            },
          ],
        },
      ],
    },
    {
      addon_guid: "addon6@tests.mozilla.org",
      version_ranges: [
        {
          addon_min_version: "0.5",
          addon_max_version: "0.9",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "1",
              max_version: "2",
            },
          ],
        },
      ],
    },
    {
      addon_guid: "addon7@tests.mozilla.org",
      version_ranges: [
        {
          addon_min_version: "0.5",
          addon_max_version: "1.0",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "0.1",
              max_version: "0.9",
            },
          ],
        },
      ],
    },
    {
      addon_guid: "addon8@tests.mozilla.org",
      version_ranges: [
        {
          addon_min_version: "6",
          addon_max_version: "6.2",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "0.9",
              max_version: "9",
            },
          ],
        },
        {
          addon_min_version: "0.5",
          addon_max_version: "1.0",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "0.1",
              max_version: "9",
            },
            {
              name: "Unknown app",
              guid: "unknown-app@tests.mozilla.org",
              min_version: "0.1",
              max_version: "9",
            }
          ],
        },
        {
          addon_min_version: "0.1",
          addon_max_version: "0.2",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "0.1",
              max_version: "0.9",
            },
          ],
        },
      ],
    },
    {
      addon_guid: "addon9@tests.mozilla.org",
      version_ranges: [
        {
          addon_min_version: "0.5",
          addon_max_version: "1.0",
          applications: [
            {
              name: "XPCShell",
              guid: "xpcshell@tests.mozilla.org",
              min_version: "1",
              max_version: "2",
            },
          ],
        },
      ],
    },
  ],
};
gServer.registerPathHandler("/compat.json", (request, response) => {
  response.setHeader("content-type", "application/json");
  response.write(JSON.stringify(COMPAT_RESPONSE));
});

Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, `${BASE_URL}/addons.json`);
Services.prefs.setCharPref(PREF_COMPAT_OVERRIDES, `${BASE_URL}/compat.json`);


// Not hosted, no overrides
var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 1",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, no overrides
var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 2",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, matching override
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 3",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, matching override, wouldn't be compatible if strict checking is enabled
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 4",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0.1",
    maxVersion: "0.2"
  }]
};

// Hosted, app ID doesn't match in override
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 5",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, addon version range doesn't match in override
var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 6",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, app version range doesn't match in override
var addon7 = {
  id: "addon7@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 7",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, multiple overrides
var addon8 = {
  id: "addon8@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 8",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Not hosted, matching override
var addon9 = {
  id: "addon9@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 9",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(addon6, profileDir);
  writeInstallRDFForExtension(addon7, profileDir);
  writeInstallRDFForExtension(addon8, profileDir);
  writeInstallRDFForExtension(addon9, profileDir);

  startupManager();

  AddonManagerInternal.backgroundUpdateCheck().then(run_test_1);
}

function end_test() {
  gServer.stop(do_test_finished);
}

async function check_compat_status(aCallback) {
  let [a1, a2, a3, a4, a5, a6, a7, a8, a9] = await AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                                                                "addon2@tests.mozilla.org",
                                                                                "addon3@tests.mozilla.org",
                                                                                "addon4@tests.mozilla.org",
                                                                                "addon5@tests.mozilla.org",
                                                                                "addon6@tests.mozilla.org",
                                                                                "addon7@tests.mozilla.org",
                                                                                "addon8@tests.mozilla.org",
                                                                                "addon9@tests.mozilla.org"]);
  Assert.notEqual(a1, null);
  Assert.equal(AddonRepository.getCompatibilityOverridesSync(a1.id), null);
  Assert.ok(a1.isCompatible);
  Assert.ok(!a1.appDisabled);

  Assert.notEqual(a2, null);
  Assert.equal(AddonRepository.getCompatibilityOverridesSync(a2.id), null);
  Assert.ok(a2.isCompatible);
  Assert.ok(!a2.appDisabled);

  Assert.notEqual(a3, null);
  let overrides = AddonRepository.getCompatibilityOverridesSync(a3.id);
  Assert.notEqual(overrides, null);
  Assert.equal(overrides.length, 1);
  Assert.ok(!a3.isCompatible);
  Assert.ok(a3.appDisabled);

  Assert.notEqual(a4, null);
  overrides = AddonRepository.getCompatibilityOverridesSync(a4.id);
  Assert.notEqual(overrides, null);
  Assert.equal(overrides.length, 1);
  Assert.ok(!a4.isCompatible);
  Assert.ok(a4.appDisabled);

  Assert.notEqual(a5, null);
  Assert.equal(AddonRepository.getCompatibilityOverridesSync(a5.id), null);
  Assert.ok(a5.isCompatible);
  Assert.ok(!a5.appDisabled);

  Assert.notEqual(a6, null);
  overrides = AddonRepository.getCompatibilityOverridesSync(a6.id);
  Assert.notEqual(overrides, null);
  Assert.equal(overrides.length, 1);
  Assert.ok(a6.isCompatible);
  Assert.ok(!a6.appDisabled);

  Assert.notEqual(a7, null);
  overrides = AddonRepository.getCompatibilityOverridesSync(a7.id);
  Assert.notEqual(overrides, null);
  Assert.equal(overrides.length, 1);
  Assert.ok(a7.isCompatible);
  Assert.ok(!a7.appDisabled);

  Assert.notEqual(a8, null);
  overrides = AddonRepository.getCompatibilityOverridesSync(a8.id);
  Assert.notEqual(overrides, null);
  Assert.equal(overrides.length, 3);
  Assert.ok(!a8.isCompatible);
  Assert.ok(a8.appDisabled);

  Assert.notEqual(a9, null);
  overrides = AddonRepository.getCompatibilityOverridesSync(a9.id);
  Assert.notEqual(overrides, null);
  Assert.equal(overrides.length, 1);
  Assert.ok(!a9.isCompatible);
  Assert.ok(a9.appDisabled);

  executeSoon(aCallback);
}

function run_test_1() {
  info("Run test 1");
  check_compat_status(run_test_2);
}

function run_test_2() {
  info("Run test 2");
  restartManager();
  check_compat_status(end_test);
}
