/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we rebuild something sensible from a corrupt database

// Create and configure the HTTP server.
var testserver = createHttpServer({ hosts: ["example.com"] });

// register files with server
testserver.registerDirectory("/data/", do_get_file("data"));

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const ADDONS = {
  // Will get a compatibility update and stay enabled
  "addon3@tests.mozilla.org": {
    manifest: {
      name: "Test 3",
      applications: {
        gecko: {
          id: "addon3@tests.mozilla.org",
          update_url: "http://example.com/data/test_corrupt.json",
        },
      },
    },
    findUpdates: true,
    desiredState: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  // Will get a compatibility update and be enabled
  "addon4@tests.mozilla.org": {
    manifest: {
      name: "Test 4",
      applications: {
        gecko: {
          id: "addon4@tests.mozilla.org",
          update_url: "http://example.com/data/test_corrupt.json",
        },
      },
    },
    initialState: {
      userDisabled: true,
    },
    findUpdates: true,
    desiredState: {
      isActive: false,
      userDisabled: true,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  "addon5@tests.mozilla.org": {
    manifest: {
      name: "Test 5",
      applications: { gecko: { id: "addon5@tests.mozilla.org" } },
    },
    desiredState: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  "addon7@tests.mozilla.org": {
    manifest: {
      name: "Test 7",
      applications: { gecko: { id: "addon7@tests.mozilla.org" } },
    },
    initialState: {
      userDisabled: true,
    },
    desiredState: {
      isActive: false,
      userDisabled: true,
      appDisabled: false,
      pendingOperations: 0,
    },
  },

  // The default theme
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
    desiredState: {
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
    desiredState: {
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
    checkAddon(id, addons.get(id), addon.desiredState);
  }

  await promiseShutdownManager();
});

add_task(async function test_after_corruption() {
  // Shutdown and replace the database with a corrupt file (a directory
  // serves this purpose). On startup the add-ons manager won't rebuild
  // because there is a file there still.
  gExtensionsJSON.remove(true);
  gExtensionsJSON.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  await promiseStartupManager();

  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  await AddonManagerPrivate.databaseReady;

  // Accessing the add-ons should open and recover the database
  info("Test add-on state after corruption");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id), addon.desiredState);
  }

  await Assert.rejects(
    promiseShutdownManager(),
    /NotAllowedError: Could not open the file at .+ for writing/
  );
});

add_task(async function test_after_second_restart() {
  await promiseStartupManager();

  info("Test add-on state after second restart");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id), addon.desiredState);
  }

  await Assert.rejects(
    promiseShutdownManager(),
    /NotAllowedError: Could not open the file at .+ for writing/
  );
});
