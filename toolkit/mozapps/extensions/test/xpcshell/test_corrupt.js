/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we rebuild something sensible from a corrupt database


// Create and configure the HTTP server.
var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

// register files with server
testserver.registerDirectory("/addons/", do_get_file("addons"));
testserver.registerDirectory("/data/", do_get_file("data"));

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

const ADDONS = {
  // Will get a compatibility update and stay enabled
  "addon3@tests.mozilla.org": {
    "install.rdf": {
      id: "addon3@tests.mozilla.org",
      version: "1.0",
      name: "Test 3",
      bootstrap: true,
      updateURL: "http://example.com/data/test_corrupt.json",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },
    findUpdates: true,
    desiredState: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
    // The compatibility update won't be recovered but it should still be
    // active for this session
    afterCorruption: {},
    afterSecondRestart: {},
  },

  // Will get a compatibility update and be enabled
  "addon4@tests.mozilla.org": {
    "install.rdf": {
      id: "addon4@tests.mozilla.org",
      version: "1.0",
      name: "Test 4",
      bootstrap: true,
      updateURL: "http://example.com/data/test_corrupt.json",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
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
    // The compatibility update won't be recovered and with strict
    // compatibility it would not have been able to tell that it was
    // previously userDisabled. However, without strict compat, it wasn't
    // appDisabled, so it knows it must have been userDisabled.
    afterCorruption: {},
    afterSecondRestart: {},
  },

  // Would stay incompatible with strict compat
  "addon5@tests.mozilla.org": {
    "install.rdf": {
      id: "addon5@tests.mozilla.org",
      version: "1.0",
      name: "Test 5",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },
    desiredState: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

  // Enabled bootstrapped
  "addon6@tests.mozilla.org": {
    "install.rdf": {
      id: "addon6@tests.mozilla.org",
      version: "1.0",
      name: "Test 6",
      bootstrap: "true",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "2",
        maxVersion: "2"
      }]
    },
    desiredState: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

  // Disabled bootstrapped
  "addon7@tests.mozilla.org": {
    "install.rdf": {
      id: "addon7@tests.mozilla.org",
      version: "1.0",
      name: "Test 7",
      bootstrap: "true",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "2",
        maxVersion: "2"
      }]
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
    afterCorruption: {},
    afterSecondRestart: {},
  },

  // The default theme
  "theme1@tests.mozilla.org": {
    manifest: {
      manifest_version: 2,
      name: "Theme 1",
      version: "1.0",
      theme: { images: { headerURL: "example.png" } },
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
    afterCorruption: {},
    afterSecondRestart: {},
  },

  "theme2@tests.mozilla.org": {
    manifest: {
      manifest_version: 2,
      name: "Theme 2",
      version: "1.0",
      theme: { images: { headerURL: "example.png" } },
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
    afterCorruption: {},
    afterSecondRestart: {},
  },
};

const IDS = Object.keys(ADDONS);

function promiseUpdates(addon) {
  return new Promise(resolve => {
    addon.findUpdates({onUpdateFinished: resolve},
                      AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
  });
}

const profileDir = gProfD.clone();
profileDir.append("extensions");

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  for (let addon of Object.values(ADDONS)) {
    if (addon["install.rdf"]) {
      await promiseWriteInstallRDFForExtension(addon["install.rdf"], profileDir);
    } else {
      let webext = createTempWebExtensionFile({manifest: addon.manifest});
      await AddonTestUtils.manuallyInstall(webext);
    }
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

  await new Promise(resolve => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, "xpi-database-loaded");
      resolve();
    }, "xpi-database-loaded");
    Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  });

  // Accessing the add-ons should open and recover the database
  info("Test add-on state after corruption");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id),
               Object.assign({}, addon.desiredState, addon.afterCorruption));
  }

  await Assert.rejects(promiseShutdownManager(), OS.File.Error);
});

add_task(async function test_after_second_restart() {
  await promiseStartupManager();

  info("Test add-on state after second restart");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id),
               Object.assign({}, addon.desiredState, addon.afterSecondRestart));
  }

  await Assert.rejects(promiseShutdownManager(), OS.File.Error);
});
