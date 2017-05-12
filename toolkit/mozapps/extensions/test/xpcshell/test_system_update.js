// Tests that system add-on upgrades work.

Components.utils.import("resource://testing-common/httpd.js");

BootstrapMonitor.init();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

var testserver = new HttpServer();
testserver.registerDirectory("/data/", do_get_file("data/system_addons"));
testserver.start();
var root = testserver.identity.primaryScheme + "://" +
           testserver.identity.primaryHost + ":" +
           testserver.identity.primaryPort + "/data/"
Services.prefs.setCharPref(PREF_SYSTEM_ADDON_UPDATE_URL, root + "update.xml");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"], true);
registerDirectory("XREAppFeat", distroDir);
initSystemAddonDirs();

/**
 * Defines the set of initial conditions to run each test against. Each should
 * define the following properties:
 *
 * setup:        A task to setup the profile into the initial state.
 * initialState: The initial expected system add-on state after setup has run.
 */
const TEST_CONDITIONS = {
  // Runs tests with no updated or default system add-ons initially installed
  blank: {
    setup() {
      clearSystemAddonUpdatesDir();
      distroDir.leafName = "empty";
    },
    initialState: [
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: null}
    ],
  },
  // Runs tests with default system add-ons installed
  withAppSet: {
    setup() {
      clearSystemAddonUpdatesDir();
      distroDir.leafName = "prefilled";
    },
    initialState: [
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: "2.0"},
      { isUpgrade: false, version: "2.0"},
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: null}
    ]
  },

  // Runs tests with updated system add-ons installed
  withProfileSet: {
    setup() {
      buildPrefilledUpdatesDir();
      distroDir.leafName = "empty";
    },
    initialState: [
      { isUpgrade: false, version: null},
      { isUpgrade: true, version: "2.0"},
      { isUpgrade: true, version: "2.0"},
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: null}
    ]
  },

  // Runs tests with both default and updated system add-ons installed
  withBothSets: {
    setup() {
      buildPrefilledUpdatesDir();
      distroDir.leafName = "hidden";
    },
    initialState: [
      { isUpgrade: false, version: "1.0"},
      { isUpgrade: true, version: "2.0"},
      { isUpgrade: true, version: "2.0"},
      { isUpgrade: false, version: null},
      { isUpgrade: false, version: null}
    ]
  },
};


/**
 * The tests to run. Each test must define an updateList or test. The following
 * properties are used:
 *
 * updateList: The set of add-ons the server should respond with.
 * test:       A function to run to perform the update check (replaces
 *             updateList)
 * fails:      An optional property, if true the update check is expected to
 *             fail.
 * finalState: An optional property, the expected final state of system add-ons,
 *             if missing the test condition's initialState is used.
 */
const TESTS = {
  // Test that a blank response does nothing
  blank: {
    updateList: null,
  },

  // Test that an empty list removes existing updates, leaving defaults.
  empty: {
    updateList: [],
    finalState: {
      blank: [
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null}
      ],
      withAppSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: "2.0"},
        { isUpgrade: false, version: "2.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null}
      ],
      withProfileSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null}
      ],
      withBothSets: [
        { isUpgrade: false, version: "1.0"},
        { isUpgrade: false, version: "1.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        // Set this to `true` to so `verifySystemAddonState()` expects a blank profile dir
        { isUpgrade: true, version: null}
      ]
    },
  },
  // Tests that a new set of system add-ons gets installed
  newset: {
    updateList: [
      { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" },
      { id: "system5@tests.mozilla.org", version: "1.0", path: "system5_1.xpi" }
    ],
    finalState: {
      blank: [
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: true, version: "1.0"}
      ],
      withAppSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: "2.0"},
        { isUpgrade: false, version: "2.0"},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: true, version: "1.0"}
      ],
      withProfileSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: true, version: "1.0"}
      ],
      withBothSets: [
        { isUpgrade: false, version: "1.0"},
        { isUpgrade: false, version: "1.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: true, version: "1.0"}
      ]
    }
  },

  // Tests that an upgraded set of system add-ons gets installed
  upgrades: {
    updateList: [
      { id: "system2@tests.mozilla.org", version: "3.0", path: "system2_3.xpi" },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi" }
    ],
    finalState: {
      blank: [
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null}
      ],
      withAppSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null}
      ],
      withProfileSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null}
      ],
      withBothSets: [
        { isUpgrade: false, version: "1.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: false, version: null}
      ]
    }
  },

  // Tests that a set of system add-ons, some new, some existing gets installed
  overlapping: {
    updateList: [
      { id: "system1@tests.mozilla.org", version: "2.0", path: "system1_2.xpi" },
      { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi" },
      { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" }
    ],
    finalState: {
      blank: [
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: false, version: null}
      ],
      withAppSet: [
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: false, version: null}
      ],
      withProfileSet: [
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: false, version: null}
      ],
      withBothSets: [
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "2.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "1.0"},
        { isUpgrade: false, version: null}
      ]
    }
  },

  // Correct sizes and hashes should work
  checkSizeHash: {
    updateList: [
      { id: "system2@tests.mozilla.org", version: "3.0", path: "system2_3.xpi", size: 4697 },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi", hashFunction: "sha1", hashValue: "a4c7198d56deb315511c02937fd96c696de6cb84" },
      { id: "system5@tests.mozilla.org", version: "1.0", path: "system5_1.xpi", size: 4691, hashFunction: "sha1", hashValue: "6887b916a1a9a5338b0df4181f6187f5396861eb" }
    ],
    finalState: {
      blank: [
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "1.0"}
      ],
      withAppSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "1.0"}
      ],
      withProfileSet: [
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "1.0"}
      ],
      withBothSets: [
        { isUpgrade: false, version: "1.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: true, version: "3.0"},
        { isUpgrade: false, version: null},
        { isUpgrade: true, version: "1.0"}
      ]
    }
  }
}

add_task(async function setup() {
  // Initialise the profile
  startupManager();
  await promiseShutdownManager();
});

add_task(async function() {
  for (let setupName of Object.keys(TEST_CONDITIONS)) {
    for (let testName of Object.keys(TESTS)) {
        do_print("Running test " + setupName + " " + testName);

        let setup = TEST_CONDITIONS[setupName];
        let test = TESTS[testName];

        await execSystemAddonTest(setupName, setup, test, distroDir, root, testserver);
    }
  }
});

// Some custom tests
// Test that the update check is performed as part of the regular add-on update
// check
add_task(async function test_addon_update() {
  await setupSystemAddonConditions(TEST_CONDITIONS.blank, distroDir);

  await updateAllSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ], root), testserver);

  await verifySystemAddonState(TEST_CONDITIONS.blank.initialState, [
    {isUpgrade: false, version: null},
    {isUpgrade: true, version: "2.0"},
    {isUpgrade: true, version: "2.0"},
    {isUpgrade: false, version: null},
    {isUpgrade: false, version: null}
  ], false, distroDir);

  await promiseShutdownManager();
});

// Disabling app updates should block system add-on updates
add_task(async function test_app_update_disabled() {
  await setupSystemAddonConditions(TEST_CONDITIONS.blank, distroDir);

  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, false);
  await updateAllSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ], root), testserver);
  Services.prefs.clearUserPref(PREF_APP_UPDATE_ENABLED);

  await verifySystemAddonState(TEST_CONDITIONS.blank.initialState, undefined, false, distroDir);

  await promiseShutdownManager();
});

// Safe mode should block system add-on updates
add_task(async function test_safe_mode() {
  gAppInfo.inSafeMode = true;

  await setupSystemAddonConditions(TEST_CONDITIONS.blank, distroDir);

  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, false);
  await updateAllSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ], root), testserver);
  Services.prefs.clearUserPref(PREF_APP_UPDATE_ENABLED);

  await verifySystemAddonState(TEST_CONDITIONS.blank.initialState, undefined, false, distroDir);

  await promiseShutdownManager();

  gAppInfo.inSafeMode = false;
});

// Tests that a set that matches the default set does nothing
add_task(async function test_match_default() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withAppSet, distroDir);

  await installSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ], root), testserver);

  // Shouldn't have installed an updated set
  await verifySystemAddonState(TEST_CONDITIONS.withAppSet.initialState, undefined, false, distroDir);

  await promiseShutdownManager();
});

// Tests that a set that matches the hidden default set works
add_task(async function test_match_default_revert() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  await installSystemAddons(await buildSystemAddonUpdates([
    { id: "system1@tests.mozilla.org", version: "1.0", path: "system1_1.xpi" },
    { id: "system2@tests.mozilla.org", version: "1.0", path: "system2_1.xpi" }
  ], root), testserver);

  // This should revert to the default set instead of installing new versions
  // into an updated set.
  await verifySystemAddonState(TEST_CONDITIONS.withBothSets.initialState, [
    {isUpgrade: false, version: "1.0"},
    {isUpgrade: false, version: "1.0"},
    {isUpgrade: false, version: null},
    {isUpgrade: false, version: null},
    {isUpgrade: false, version: null}
  ], false, distroDir);

  await promiseShutdownManager();
});

// Tests that a set that matches the current set works
add_task(async function test_match_current() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  await installSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ], root), testserver);

  // This should remain with the current set instead of creating a new copy
  let set = JSON.parse(Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET));
  do_check_eq(set.directory, "prefilled");

  await verifySystemAddonState(TEST_CONDITIONS.withBothSets.initialState, undefined, false, distroDir);

  await promiseShutdownManager();
});

// Tests that a set with a minor change doesn't re-download existing files
add_task(async function test_no_download() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  // The missing file here is unneeded since there is a local version already
  await installSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "missing.xpi" },
    { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" }
  ], root), testserver);

  await verifySystemAddonState(TEST_CONDITIONS.withBothSets.initialState, [
    {isUpgrade: false, version: "1.0"},
    {isUpgrade: true, version: "2.0"},
    {isUpgrade: false, version: null},
    {isUpgrade: true, version: "1.0"},
    {isUpgrade: false, version: null}
  ], false, distroDir);

  await promiseShutdownManager();
});

// Tests that a second update before a restart works
add_task(async function test_double_update() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withAppSet, distroDir);

  await installSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "1.0", path: "system3_1.xpi" }
  ], root), testserver);

  await installSystemAddons(await buildSystemAddonUpdates([
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" },
    { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" }
  ], root), testserver);

  await verifySystemAddonState(TEST_CONDITIONS.withAppSet.initialState, [
    {isUpgrade: false, version: null},
    {isUpgrade: false, version: "2.0"},
    {isUpgrade: true, version: "2.0"},
    {isUpgrade: true, version: "1.0"},
    {isUpgrade: false, version: null}
  ], true, distroDir);

  await promiseShutdownManager();
});

// A second update after a restart will delete the original unused set
add_task(async function test_update_purges() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  await installSystemAddons(await buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "1.0", path: "system3_1.xpi" }
  ], root), testserver);

  await verifySystemAddonState(TEST_CONDITIONS.withBothSets.initialState, [
    {isUpgrade: false, version: "1.0"},
    {isUpgrade: true, version: "2.0"},
    {isUpgrade: true, version: "1.0"},
    {isUpgrade: false, version: null},
    {isUpgrade: false, version: null}
  ], false, distroDir);

  await installSystemAddons(await buildSystemAddonUpdates(null), testserver);

  let dirs = await getSystemAddonDirectories();
  do_check_eq(dirs.length, 1);

  await promiseShutdownManager();
});
