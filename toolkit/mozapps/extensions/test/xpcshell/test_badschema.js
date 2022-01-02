/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we rebuild something sensible from a database with a bad schema

var testserver = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

// register files with server
testserver.registerDirectory("/data/", do_get_file("data"));

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const ADDONS = {
  "addon1@tests.mozilla.org": {
    manifest: {
      name: "Test 1",
      applications: {
        gecko: {
          id: "addon1@tests.mozilla.org",
          strict_min_version: "2",
          strict_max_version: "2",
        },
      },
    },
    desiredValues: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  "addon2@tests.mozilla.org": {
    manifest: {
      name: "Test 2",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon2@tests.mozilla.org",
        },
      },
    },
    initialState: {
      userDisabled: true,
    },
    desiredValues: {
      isActive: false,
      userDisabled: true,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  "addon3@tests.mozilla.org": {
    manifest: {
      name: "Test 3",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon3@tests.mozilla.org",
          update_url: "http://example.com/data/test_corrupt.json",
          strict_min_version: "1",
          strict_max_version: "1",
        },
      },
    },
    findUpdates: true,
    desiredValues: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  "addon4@tests.mozilla.org": {
    manifest: {
      name: "Test 4",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon4@tests.mozilla.org",
          update_url: "http://example.com/data/test_corrupt.json",
          strict_min_version: "1",
          strict_max_version: "1",
        },
      },
    },
    initialState: {
      userDisabled: true,
    },
    findUpdates: true,
    desiredValues: {
      isActive: false,
      userDisabled: true,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  "addon5@tests.mozilla.org": {
    manifest: {
      name: "Test 5",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon5@tests.mozilla.org",
          strict_min_version: "1",
          strict_max_version: "1",
        },
      },
    },
    desiredValues: {
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      pendingOperations: 0,
    },
  },

  "theme1@tests.mozilla.org": {
    manifest: {
      manifest_version: 2,
      name: "Theme 1",
      version: "1.0",
      theme: { images: { theme_frame: "example.png" } },
      applications: {
        gecko: {
          id: "theme1@tests.mozilla.org",
        },
      },
    },
    desiredValues: {
      isActive: false,
      userDisabled: true,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  "theme2@tests.mozilla.org": {
    manifest: {
      manifest_version: 2,
      name: "Theme 2",
      version: "1.0",
      theme: { images: { theme_frame: "example.png" } },
      applications: {
        gecko: {
          id: "theme2@tests.mozilla.org",
        },
      },
    },
    initialState: {
      userDisabled: false,
    },
    desiredValues: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
  },
};

const IDS = Object.keys(ADDONS);

function promiseUpdates(addon) {
  return new Promise(resolve => {
    addon.findUpdates(
      { onUpdateFinished: resolve },
      AddonManager.UPDATE_WHEN_PERIODIC_UPDATE
    );
  });
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  for (let addon of Object.values(ADDONS)) {
    let webext = createTempWebExtensionFile({ manifest: addon.manifest });
    await AddonTestUtils.manuallyInstall(webext);
  }

  await promiseStartupManager();

  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    if (addon.initialState) {
      await setInitialState(addons.get(id), addon.initialState);
    }
    if (addon.findUpdates) {
      await promiseUpdates(addons.get(id));
    }
  }
});

add_task(async function test_after_restart() {
  await promiseRestartManager();

  info("Test add-on state after restart");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id), addon.desiredValues);
  }

  await promiseShutdownManager();
});

add_task(async function test_after_schema_version_change() {
  // After restarting the database won't be open so we can alter
  // the schema
  await changeXPIDBVersion(100);

  await promiseStartupManager();

  info("Test add-on state after schema version change");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id), addon.desiredValues);
  }

  await promiseShutdownManager();
});

add_task(async function test_after_second_restart() {
  await promiseStartupManager();

  info("Test add-on state after second restart");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id), addon.desiredValues);
  }

  await promiseShutdownManager();
});
