/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we rebuild something sensible from a database with a bad schema

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

// register files with server
testserver.registerDirectory("/addons/", do_get_file("addons"));
testserver.registerDirectory("/data/", do_get_file("data"));

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

const ADDONS = {
  "addon1@tests.mozilla.org": {
    "install.rdf": {
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      name: "Test 1",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "2",
        maxVersion: "2"
      }]
    },
    desiredValues: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

  "addon2@tests.mozilla.org": {
    "install.rdf": {
      id: "addon2@tests.mozilla.org",
      version: "1.0",
      name: "Test 2",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "2",
        maxVersion: "2"
      }]
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
    afterCorruption: {},
    afterSecondRestart: {},
  },

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
    desiredValues: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

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
    desiredValues: {
      isActive: false,
      userDisabled: true,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

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
    desiredValues: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

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
    desiredValues: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

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
    desiredValues: {
      isActive: false,
      userDisabled: true,
      appDisabled: false,
      pendingOperations: 0,
    },
    afterCorruption: {},
    afterSecondRestart: {},
  },

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
    desiredValues: {
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
    desiredValues: {
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
      Object.assign(addons.get(id), addon.initialState);
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
    checkAddon(id, addons.get(id),
               Object.assign({}, addon.desiredValues, addon.afterCorruption));
  }

  await promiseShutdownManager();
});

add_task(async function test_after_second_restart() {
  await promiseStartupManager();

  info("Test add-on state after second restart");
  let addons = await getAddons(IDS);
  for (let [id, addon] of Object.entries(ADDONS)) {
    checkAddon(id, addons.get(id),
               Object.assign({}, addon.desiredValues, addon.afterSecondRestart));
  }

  await promiseShutdownManager();
});
