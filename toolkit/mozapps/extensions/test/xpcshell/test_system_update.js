// Tests that we reset to the default system add-ons correctly when switching
// application versions
const PREF_SYSTEM_ADDON_SET           = "extensions.systemAddonSet";
const PREF_SYSTEM_ADDON_UPDATE_URL    = "extensions.systemAddon.update.url";
const PREF_XPI_STATE                  = "extensions.xpiState";
const PREF_APP_UPDATE_ENABLED         = "app.update.enabled";

Components.utils.import("resource://testing-common/httpd.js");
const { computeHash } = Components.utils.import("resource://gre/modules/addons/ProductAddonChecker.jsm");

BootstrapMonitor.init();

const featureDir = FileUtils.getDir("ProfD", ["features"], false);

function getCurrentFeatureDir() {
  let dir = featureDir.clone();
  let set = JSON.parse(Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET));
  dir.append(set.directory);
  return dir;
}

function clearFeatureDir() {
  // Delete any existing directories
  if (featureDir.exists())
    featureDir.remove(true);

  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_SET);
}

function buildPrefilledFeatureDir() {
  clearFeatureDir();

  // Build the test set
  let dir = FileUtils.getDir("ProfD", ["features", "prefilled"], true);

  do_get_file("data/system_addons/system2_2.xpi").copyTo(dir, "system2@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system3_2.xpi").copyTo(dir, "system3@tests.mozilla.org.xpi");

  // Mark these in the past so the startup file scan notices when files have changed properly
  FileUtils.getFile("ProfD", ["features", "prefilled", "system2@tests.mozilla.org.xpi"]).lastModifiedTime -= 10000;
  FileUtils.getFile("ProfD", ["features", "prefilled", "system3@tests.mozilla.org.xpi"]).lastModifiedTime -= 10000;

  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify({
    schema: 1,
    directory: dir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0"
      },
      "system3@tests.mozilla.org": {
        version: "2.0"
      },
    }
  }));
}

let dir = FileUtils.getDir("ProfD", ["sysfeatures", "hidden"], true);
do_get_file("data/system_addons/system1_1.xpi").copyTo(dir, "system1@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system2_1.xpi").copyTo(dir, "system2@tests.mozilla.org.xpi");

dir = FileUtils.getDir("ProfD", ["sysfeatures", "prefilled"], true);
do_get_file("data/system_addons/system2_2.xpi").copyTo(dir, "system2@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system3_2.xpi").copyTo(dir, "system3@tests.mozilla.org.xpi");

const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"], true);
registerDirectory("XREAppFeat", distroDir);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

var testserver = new HttpServer();
testserver.registerDirectory("/data/", do_get_file("data/system_addons"));
testserver.start();
var root = testserver.identity.primaryScheme + "://" +
           testserver.identity.primaryHost + ":" +
           testserver.identity.primaryPort + "/data/"
Services.prefs.setCharPref(PREF_SYSTEM_ADDON_UPDATE_URL, root + "update.xml");

function makeUUID() {
  let uuidGen = AM_Cc["@mozilla.org/uuid-generator;1"].
                getService(AM_Ci.nsIUUIDGenerator);
  return uuidGen.generateUUID().toString();
}

function* serve_update(xml, perform_update) {
  testserver.registerPathHandler("/data/update.xml", (request, response) => {
    response.write(xml);
  });

  try {
    yield perform_update();
  }
  finally {
    testserver.registerPathHandler("/data/update.xml", null);
  }
}

// Runs an update check making it use the passed in xml string. Uses the direct
// call to the update function so we get rejections on failure.
function* install_system_addons(xml) {
  do_print("Triggering system add-on update check.");

  yield serve_update(xml, function*() {
    let { XPIProvider } = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm");
    yield XPIProvider.updateSystemAddons();
  });
}

// Runs a full add-on update check which will in some cases do a system add-on
// update check. Always succeeds.
function* update_all_addons(xml) {
  do_print("Triggering full add-on update check.");

  yield serve_update(xml, function() {
    return new Promise(resolve => {
      Services.obs.addObserver(function() {
        Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");

        resolve();
      }, "addons-background-update-complete", false);

      // Trigger the background update timer handler
      gInternalManager.notify(null);
    });
  });
}

// Builds an update.xml file for an update check based on the data passed.
function* build_xml(addons) {
  let xml = `<?xml version="1.0" encoding="UTF-8"?>\n\n<updates>\n`;
  if (addons) {
    xml += `  <addons>\n`;
    for (let addon of addons) {
      xml += `    <addon id="${addon.id}" URL="${root + addon.path}" version="${addon.version}"`;
      if (addon.size)
        xml += ` size="${addon.size}"`;
      if (addon.hashFunction)
        xml += ` hashFunction="${addon.hashFunction}"`;
      if (addon.hashValue)
        xml += ` hashValue="${addon.hashValue}"`;
      xml += `/>\n`;
    }
    xml += `  </addons>\n`;
  }
  xml += `</updates>\n`;

  return xml;
}

function* check_installed(inProfile, ...versions) {
  let expectedDir = inProfile ? getCurrentFeatureDir() : distroDir;

  for (let i = 0; i < versions.length; i++) {
    let id = "system" + (i + 1) + "@tests.mozilla.org";
    let addon = yield promiseAddonByID(id);

    if (versions[i]) {
      do_print(`Checking state of add-on ${id}, expecting version ${versions[i]}`);

      // Add-on should be installed
      do_check_neq(addon, null);
      do_check_eq(addon.version, versions[i]);
      do_check_true(addon.isActive);
      do_check_false(addon.foreignInstall);
      do_check_true(addon.hidden);
      do_check_true(addon.isSystem);

      // Verify the add-ons file is in the right place
      let file = expectedDir.clone();
      file.append(id + ".xpi");
      do_check_true(file.exists());
      do_check_true(file.isFile());

      let uri = addon.getResourceURI(null);
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      do_check_eq(uri.file.path, file.path);

      if (inProfile) {
        do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_SYSTEM);
      }

      // Verify the add-on actually started
      BootstrapMonitor.checkAddonStarted(id, versions[i]);
    }
    else {
      do_print(`Checking state of add-on ${id}, expecting it to be missing`);

      if (inProfile) {
        // Add-on should not be installed
        do_check_eq(addon, null);
      }
      else {
        // Either add-on should not be installed or it shouldn't be active
        do_check_true(!addon || !addon.isActive);
      }

      BootstrapMonitor.checkAddonNotStarted(id);

      if (addon)
        BootstrapMonitor.checkAddonInstalled(id);
      else
        BootstrapMonitor.checkAddonNotInstalled(id);
    }
  }
}


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
    setup: function*() {
      clearFeatureDir();
      distroDir.leafName = "empty";
    },
    initialState: [false, null, null, null, null, null],
  },

  // Runs tests with default system add-ons installed
  withAppSet: {
    setup: function*() {
      clearFeatureDir();
      distroDir.leafName = "prefilled";
    },
    initialState: [false, null, "2.0", "2.0", null, null],
  },

  // Runs tests with updated system add-ons installed
  withProfileSet: {
    setup: function*() {
      buildPrefilledFeatureDir();
      distroDir.leafName = "empty";
    },
    initialState: [true, null, "2.0", "2.0", null, null],
  },

  // Runs tests with both default and updated system add-ons installed
  withBothSets: {
    setup: function*() {
      buildPrefilledFeatureDir();
      distroDir.leafName = "hidden";
    },
    initialState: [true, null, "2.0", "2.0", null, null],
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
  // Test that an error response does nothing
  error: {
    test: function*() {
      try {
        yield install_system_addons("foobar");
        do_throw("Expected to fail the update check");
      }
      catch (e) {
        do_check_true(true, "Expected to fail the update check");
      }
    },
  },

  // Test that a blank response does nothing
  blank: {
    updateList: null,
  },

  // Test that an empty list updates to an empty set of system add-ons
  // FIXME - temporarily disabled, see bug 1291569.
  /*
  empty: {
    updateList: [],
    finalState: [false, null, null, null, null, null]
  },
  */
  // Tests that a new set of system add-ons gets installed
  newset: {
    updateList: [
      { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" },
      { id: "system5@tests.mozilla.org", version: "1.0", path: "system5_1.xpi" }
    ],
    finalState: [true, null, null, null, "1.0", "1.0"]
  },

  // Tests that an upgraded set of system add-ons gets installed
  upgrades: {
    updateList: [
      { id: "system2@tests.mozilla.org", version: "3.0", path: "system2_3.xpi" },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi" }
    ],
    finalState: [true, null, "3.0", "3.0", null, null]
  },

  // Tests that a set of system add-ons, some new, some existing gets installed
  // FIXME - these XPIs need to be set to 1.0 and signed again.
  // See bug 1291569.
  overlapping: {
    updateList: [
      { id: "system1@tests.mozilla.org", version: "2.0", path: "system1_2.xpi" },
      { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi" },
      { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" }
    ],
    finalState: [true, "2.0", "2.0", "3.0", "1.0", null]
  },

  // Specifying an incorrect version should stop us updating anything
  badVersion: {
    fails: true,
    updateList: [
      { id: "system2@tests.mozilla.org", version: "4.0", path: "system2_3.xpi" },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi" }
    ],
  },

  // Specifying an invalid size should stop us updating anything
  badSize: {
    fails: true,
    updateList: [
      { id: "system2@tests.mozilla.org", version: "3.0", path: "system2_3.xpi", size: 2 },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi" }
    ],
  },

  // Specifying an incorrect hash should stop us updating anything
  badHash: {
    fails: true,
    updateList: [
      { id: "system2@tests.mozilla.org", version: "3.0", path: "system2_3.xpi" },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi", hashFunction: "sha1", hashValue: "205a4c49bd513ebd30594e380c19e86bba1f83e2" }
    ],
  },

  // Correct sizes and hashes should work
  checkSizeHash: {
    updateList: [
      { id: "system2@tests.mozilla.org", version: "3.0", path: "system2_3.xpi", size: 4672 },
      { id: "system3@tests.mozilla.org", version: "3.0", path: "system3_3.xpi", hashFunction: "sha1", hashValue: "2df604b37b13766c0e04f1b7f59800e038f46cd5" },
      { id: "system5@tests.mozilla.org", version: "1.0", path: "system5_1.xpi", size: 4671, hashFunction: "sha1", hashValue: "f13dcaa8bfacaa222189bcbb0074972c05ceb621" }
    ],
    finalState: [true, null, "3.0", "3.0", null, "1.0"]
  },

  // A bad certificate should stop updates
  badCert: {
    fails: true,
    updateList: [
      { id: "system1@tests.mozilla.org", version: "1.0", path: "system1_1_badcert.xpi" },
      { id: "system3@tests.mozilla.org", version: "1.0", path: "system3_1.xpi" }
    ],
  }
}

add_task(function* setup() {
  // Initialise the profile
  startupManager();
  yield promiseShutdownManager();
})

function* get_directories() {
  let subdirs = [];

  if (yield OS.File.exists(featureDir.path)) {
    let iterator = new OS.File.DirectoryIterator(featureDir.path);
    yield iterator.forEach(entry => {
      if (entry.isDir) {
        subdirs.push(entry);
      }
    });
    iterator.close();
  }

  return subdirs;
}

function* setup_conditions(setup) {
  do_print("Clearing existing database.");
  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_SET);
  distroDir.leafName = "empty";
  startupManager(false);
  yield promiseShutdownManager();

  do_print("Setting up conditions.");
  yield setup.setup();

  startupManager(false);

  // Make sure the initial state is correct
  do_print("Checking initial state.");
  yield check_installed(...setup.initialState);
}

function* verify_state(initialState, finalState = undefined) {
  let expectedDirs = 0;

  // If the initial state was using the profile set then that directory will
  // still exist.
  if (initialState[0])
    expectedDirs++;

  if (finalState == undefined) {
    finalState = initialState;
  }
  else {
    // If the new state is using the profile then that directory will exist.
    if (finalState[0])
      expectedDirs++;
  }

  do_print("Checking final state.");

  let dirs = yield get_directories();
  do_check_eq(dirs.length, expectedDirs);

  // Bug 1204156: Currently switching to the new state requires a restart
  // yield check_installed(...finalState);

  // Check that the new state is active after a restart
  yield promiseRestartManager();
  yield check_installed(...finalState);
}

function* exec_test(setup, test) {
  yield setup_conditions(setup);

  try {
    if ("test" in test) {
      yield test.test();
    }
    else {
      yield install_system_addons(yield build_xml(test.updateList));
    }

    if (test.fails) {
      do_throw("Expected this test to fail");
    }
  }
  catch (e) {
    if (!test.fails) {
      do_throw(e);
    }
  }

  yield verify_state(setup.initialState, test.finalState);

  yield promiseShutdownManager();
}

add_task(function*() {
  for (let setup of Object.keys(TEST_CONDITIONS)) {
    for (let test of Object.keys(TESTS)) {
        do_print("Running test " + setup + " " + test);

        yield exec_test(TEST_CONDITIONS[setup], TESTS[test]);

    }
  }
});

// Some custom tests

// Test that the update check is performed as part of the regular add-on update
// check
add_task(function* test_addon_update() {
  yield setup_conditions(TEST_CONDITIONS.blank);

  yield update_all_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ]));

  yield verify_state(TEST_CONDITIONS.blank.initialState,
                     [true, null, "2.0", "2.0", null, null]);

  yield promiseShutdownManager();
});

// Disabling app updates should block system add-on updates
add_task(function* test_app_update_disabled() {
  yield setup_conditions(TEST_CONDITIONS.blank);

  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, false);
  yield update_all_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ]));
  Services.prefs.clearUserPref(PREF_APP_UPDATE_ENABLED);

  yield verify_state(TEST_CONDITIONS.blank.initialState);

  yield promiseShutdownManager();
});

// Safe mode should block system add-on updates
add_task(function* test_safe_mode() {
  gAppInfo.inSafeMode = true;

  yield setup_conditions(TEST_CONDITIONS.blank);

  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, false);
  yield update_all_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ]));
  Services.prefs.clearUserPref(PREF_APP_UPDATE_ENABLED);

  yield verify_state(TEST_CONDITIONS.blank.initialState);

  yield promiseShutdownManager();

  gAppInfo.inSafeMode = false;
});

// Tests that a set that matches the default set does nothing
add_task(function* test_match_default() {
  yield setup_conditions(TEST_CONDITIONS.withAppSet);

  yield install_system_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ]));

  // Shouldn't have installed an updated set
  yield verify_state(TEST_CONDITIONS.withAppSet.initialState);

  yield promiseShutdownManager();
});

// Tests that a set that matches the hidden default set works
add_task(function* test_match_default_revert() {
  yield setup_conditions(TEST_CONDITIONS.withBothSets);

  yield install_system_addons(yield build_xml([
    { id: "system1@tests.mozilla.org", version: "1.0", path: "system1_1.xpi" },
    { id: "system2@tests.mozilla.org", version: "1.0", path: "system2_1.xpi" }
  ]));

  // This should revert to the default set instead of installing new versions
  // into an updated set.
  yield verify_state(TEST_CONDITIONS.withBothSets.initialState,
                     [false, "1.0", "1.0", null, null, null]);

  yield promiseShutdownManager();
});

// Tests that a set that matches the current set works
add_task(function* test_match_current() {
  yield setup_conditions(TEST_CONDITIONS.withBothSets);

  yield install_system_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" }
  ]));

  // This should remain with the current set instead of creating a new copy
  let set = JSON.parse(Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET));
  do_check_eq(set.directory, "prefilled");

  yield verify_state(TEST_CONDITIONS.withBothSets.initialState);

  yield promiseShutdownManager();
});

// Tests that a set with a minor change doesn't re-download existing files
add_task(function* test_no_download() {
  yield setup_conditions(TEST_CONDITIONS.withBothSets);

  // The missing file here is unneeded since there is a local version already
  yield install_system_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "missing.xpi" },
    { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" }
  ]));

  yield verify_state(TEST_CONDITIONS.withBothSets.initialState,
                     [true, null, "2.0", null, "1.0", null]);

  yield promiseShutdownManager();
});

// Tests that a second update before a restart works
add_task(function* test_double_update() {
  yield setup_conditions(TEST_CONDITIONS.withAppSet);

  yield install_system_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "1.0", path: "system3_1.xpi" }
  ]));

  yield install_system_addons(yield build_xml([
    { id: "system3@tests.mozilla.org", version: "2.0", path: "system3_2.xpi" },
    { id: "system4@tests.mozilla.org", version: "1.0", path: "system4_1.xpi" }
  ]));

  yield verify_state(TEST_CONDITIONS.withAppSet.initialState,
                     [true, null, null, "2.0", "1.0", null]);

  yield promiseShutdownManager();
});

// A second update after a restart will delete the original unused set
add_task(function* test_update_purges() {
  yield setup_conditions(TEST_CONDITIONS.withBothSets);

  yield install_system_addons(yield build_xml([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "system2_2.xpi" },
    { id: "system3@tests.mozilla.org", version: "1.0", path: "system3_1.xpi" }
  ]));

  yield verify_state(TEST_CONDITIONS.withBothSets.initialState,
                     [true, null, "2.0", "1.0", null, null]);

  yield install_system_addons(yield build_xml(null));

  let dirs = yield get_directories();
  do_check_eq(dirs.length, 1);

  yield promiseShutdownManager();
});

