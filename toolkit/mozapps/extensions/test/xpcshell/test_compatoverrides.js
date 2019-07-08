/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests compatibility overrides, for when strict compatibility checking is
// disabled. See bug 693906.

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

let testserver = createHttpServer({ hosts: ["example.com"] });

const GETADDONS_RESPONSE = {
  next: null,
  previous: null,
  results: [],
};

AddonTestUtils.registerJSON(testserver, "/addons.json", GETADDONS_RESPONSE);

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
        },
      ],
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
            },
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

AddonTestUtils.registerJSON(testserver, "/compat.json", COMPAT_RESPONSE);

Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
Services.prefs.setCharPref(
  PREF_GETADDONS_BYIDS,
  "http://example.com/addons.json"
);
Services.prefs.setCharPref(
  PREF_COMPAT_OVERRIDES,
  "http://example.com/compat.json"
);

const ADDONS = [
  // Not hosted, no overrides
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
    compatible: true,
    overrides: 0,
  },

  // Hosted, no overrides
  {
    manifest: {
      id: "addon2@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: true,
    overrides: 0,
  },

  // Hosted, matching override
  {
    manifest: {
      id: "addon3@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: false,
    overrides: 1,
  },

  // Hosted, matching override,
  // wouldn't be compatible if strict checking is enabled
  {
    manifest: {
      id: "addon4@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "0.1",
          maxVersion: "0.2",
        },
      ],
    },
    compatible: false,
    overrides: 1,
  },

  // Hosted, app ID doesn't match in override
  {
    manifest: {
      id: "addon5@tests.mozilla.org",
      version: "1.0",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: true,
    overrides: 0,
  },

  // Hosted, addon version range doesn't match in override
  {
    manifest: {
      id: "addon6@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: true,
    overrides: 1,
  },

  // Hosted, app version range doesn't match in override
  {
    manifest: {
      id: "addon7@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: true,
    overrides: 1,
  },

  // Hosted, multiple overrides
  {
    manifest: {
      id: "addon8@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: false,
    overrides: 3,
  },

  // Not hosted, matching override
  {
    manifest: {
      id: "addon9@tests.mozilla.org",
      targetApplications: [
        {
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1",
        },
      ],
    },
    compatible: false,
    overrides: 1,
  },
];

add_task(async function run_tests() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  for (let addon of ADDONS) {
    let xpi = await createAddon(addon.manifest);
    await manuallyInstall(
      xpi,
      AddonTestUtils.profileExtensions,
      addon.manifest.id
    );
  }
  await promiseStartupManager();

  await AddonManagerInternal.backgroundUpdateCheck();

  async function check() {
    for (let info of ADDONS) {
      let { id } = info.manifest;
      let addon = await promiseAddonByID(id);
      Assert.notEqual(addon, null, `Found ${id}`);
      let overrides = AddonRepository.getCompatibilityOverridesSync(id);
      if (info.overrides === 0) {
        Assert.equal(overrides, null, `Got no overrides for ${id}`);
      } else {
        Assert.notEqual(overrides, null, `Got overrides for ${id}`);
        Assert.equal(
          overrides.length,
          info.overrides,
          `Got right number of overrides for ${id}`
        );
      }
      Assert.equal(
        addon.isCompatible,
        info.compatible,
        `Got expected compatibility for ${id}`
      );
      Assert.equal(
        addon.appDisabled,
        !info.compatible,
        `Got expected appDisabled for ${id}`
      );
    }
  }

  await check();

  await promiseRestartManager();

  await check();
});
