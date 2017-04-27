Components.utils.import("resource://testing-common/httpd.js");
Components.utils.import("resource://gre/modules/osfile.jsm");

var gServer;

const profileDir = gProfD.clone();
profileDir.append("extensions");

const NON_MPC_PREF = "extensions.allow-non-mpc-extensions";

Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

function build_test(multiprocessCompatible, bootstrap, updateMultiprocessCompatible) {
  return function* () {
    dump("Running test" +
      " multiprocessCompatible: " + multiprocessCompatible +
      " bootstrap: " + bootstrap +
      " updateMultiprocessCompatible: " + updateMultiprocessCompatible +
      "\n");

    let addonData = {
      id: "addon@tests.mozilla.org",
      name: "Test Add-on",
      version: "1.0",
      multiprocessCompatible,
      bootstrap,
      updateURL: "http://localhost:" + gPort + "/updaterdf",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    }

    gServer.registerPathHandler("/updaterdf", function(request, response) {
      let updateData = {};
      updateData[addonData.id] = [{
        version: "1.0",
        targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"
        }]
      }];

      if (updateMultiprocessCompatible !== undefined) {
        updateData[addonData.id][0].multiprocessCompatible = updateMultiprocessCompatible;
      }

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(createUpdateRDF(updateData));
    });

    let expectedMPC = updateMultiprocessCompatible === undefined ?
                      multiprocessCompatible :
                      updateMultiprocessCompatible;

    let xpifile = createTempXPIFile(addonData);
    let install = yield AddonManager.getInstallForFile(xpifile);
    do_check_eq(install.addon.multiprocessCompatible, !!multiprocessCompatible);
    do_check_eq(install.addon.mpcOptedOut, multiprocessCompatible === false)
    yield promiseCompleteAllInstalls([install]);

    if (!bootstrap) {
      yield promiseRestartManager();
      do_check_true(isExtensionInAddonsList(profileDir, addonData.id));
      do_check_eq(isItemMarkedMPIncompatible(addonData.id), !multiprocessCompatible);
    }

    let addon = yield promiseAddonByID(addonData.id);
    do_check_neq(addon, null);
    do_check_eq(addon.multiprocessCompatible, !!multiprocessCompatible);
    do_check_eq(addon.mpcOptedOut, multiprocessCompatible === false);

    yield promiseFindAddonUpdates(addon);

    // Should have applied the compatibility change
    do_check_eq(addon.multiprocessCompatible, !!expectedMPC);
    yield promiseRestartManager();

    addon = yield promiseAddonByID(addonData.id);
    // Should have persisted the compatibility change
    do_check_eq(addon.multiprocessCompatible, !!expectedMPC);
    if (!bootstrap) {
      do_check_true(isExtensionInAddonsList(profileDir, addonData.id));
      do_check_eq(isItemMarkedMPIncompatible(addonData.id), !multiprocessCompatible);
    }

    addon.uninstall();
    yield promiseRestartManager();

    gServer.registerPathHandler("/updaterdf", null);
  }
}

/* Builds a set of tests to run the same steps for every combination of:
 *   The add-on being restartless
 *   The initial add-on supporting multiprocess
 *   The update saying the add-on should or should not support multiprocess (or not say anything at all)
 */
for (let bootstrap of [false, true]) {
  for (let multiprocessCompatible of [undefined, false, true]) {
    for (let updateMultiprocessCompatible of [undefined, false, true]) {
      add_task(build_test(multiprocessCompatible, bootstrap, updateMultiprocessCompatible));
    }
  }
}

add_task(async function test_disable() {
  const ID_MPC = "mpc@tests.mozilla.org";
  const ID_NON_MPC = "non-mpc@tests.mozilla.org";
  const ID_DICTIONARY = "dictionary@tests.mozilla.org";

  let addonData = {
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }]
  }

  let xpi1 = createTempXPIFile(Object.assign({
    id: ID_MPC,
    multiprocessCompatible: true,
  }, addonData));
  let xpi2 = createTempXPIFile(Object.assign({
      id: ID_NON_MPC,
      multiprocessCompatible: false,
  }, addonData));
  let xpi3 = createTempXPIFile({
    id: ID_DICTIONARY,
    name: "Test Dictionary",
    version: "1.0",
    type: "64",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }]
  });

  async function testOnce(initialAllow) {
    if (initialAllow !== undefined) {
      Services.prefs.setBoolPref(NON_MPC_PREF, initialAllow);
    }

    let install1 = await AddonManager.getInstallForFile(xpi1);
    let install2 = await AddonManager.getInstallForFile(xpi2);
    let install3 = await AddonManager.getInstallForFile(xpi3);
    await promiseCompleteAllInstalls([install1, install2, install3]);

    let [addon1, addon2, addon3] = await AddonManager.getAddonsByIDs([ID_MPC, ID_NON_MPC, ID_DICTIONARY]);
    do_check_neq(addon1, null);
    do_check_eq(addon1.multiprocessCompatible, true);
    do_check_eq(addon1.appDisabled, false);

    do_check_neq(addon2, null);
    do_check_eq(addon2.multiprocessCompatible, false);
    do_check_eq(addon2.appDisabled, initialAllow === false);

    do_check_neq(addon3, null);
    do_check_eq(addon3.appDisabled, false);

    // Flip the allow-non-mpc preference
    let newValue = (initialAllow === true) ? false : true;
    Services.prefs.setBoolPref(NON_MPC_PREF, newValue);

    // the mpc extension should never become appDisabled
    do_check_eq(addon1.appDisabled, false);

    // The non-mpc extension should become disabled if we don't allow non-mpc
    do_check_eq(addon2.appDisabled, !newValue);

    // A non-extension (eg a dictionary) should not become disabled
    do_check_eq(addon3.appDisabled, false);

    // Flip the pref back and check appDisabled
    Services.prefs.setBoolPref(NON_MPC_PREF, !newValue);

    do_check_eq(addon1.appDisabled, false);
    do_check_eq(addon2.appDisabled, newValue);
    do_check_eq(addon3.appDisabled, false);

    addon1.uninstall();
    addon2.uninstall();
    addon3.uninstall();
  }

  await testOnce(undefined);
  await testOnce(true);
  await testOnce(false);

  Services.prefs.clearUserPref(NON_MPC_PREF);
});

// Test that the nonMpcDisabled flag gets set properly at startup
// when the allow-non-mpc-extensions pref is flipped.
add_task(async function test_restart() {
  const ID = "non-mpc@tests.mozilla.org";

  let xpifile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    multiprocessCompatible: false,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  });

  Services.prefs.setBoolPref(NON_MPC_PREF, true);
  let install = await AddonManager.getInstallForFile(xpifile);
  await promiseCompleteAllInstalls([install]);

  let addon = await AddonManager.getAddonByID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.multiprocessCompatible, false);
  do_check_eq(addon.appDisabled, false);

  // Simulate a new app version in which the allow-non-mpc-extensions
  // pref is flipped.
  await promiseShutdownManager();
  Services.prefs.setBoolPref(NON_MPC_PREF, false);
  gAppInfo.version = "1.5";
  await promiseStartupManager();

  addon = await AddonManager.getAddonByID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.appDisabled, true);

  // The flag we use for startup notification should be true
  do_check_eq(AddonManager.nonMpcDisabled, true);

  addon.uninstall();

  Services.prefs.clearUserPref(NON_MPC_PREF);
  AddonManagerPrivate.nonMpcDisabled = false;
});

// Test that the nonMpcDisabled flag is not set if there are non-mpc
// extensions that are also disabled for some other reason.
add_task(async function test_restart2() {
  const ID1 = "blocked@tests.mozilla.org";
  let xpi1 = createTempXPIFile({
    id: ID1,
    name: "Blocked Add-on",
    version: "1.0",
    bootstrap: true,
    multiprocessCompatible: false,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  });

  const ID2 = "incompatible@tests.mozilla.org";
  let xpi2 = createTempXPIFile({
    id: ID2,
    name: "Incompatible Add-on",
    version: "1.0",
    bootstrap: true,
    multiprocessCompatible: false,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.5"
    }]
  });

  const BLOCKLIST = `<?xml version="1.0"?>
  <blocklist xmlns="http://www.mozilla.org/2006/addons-blocklist" lastupdate="1396046918000">
  <emItems>
  <emItem  blockID="i454" id="${ID1}">
  <versionRange  minVersion="0" maxVersion="*" severity="3"/>
  </emItem>
  </emItems>
  </blocklist>`;


  Services.prefs.setBoolPref(NON_MPC_PREF, true);
  let install1 = await AddonManager.getInstallForFile(xpi1);
  let install2 = await AddonManager.getInstallForFile(xpi2);
  await promiseCompleteAllInstalls([install1, install2]);

  let [addon1, addon2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  do_check_neq(addon1, null);
  do_check_eq(addon1.multiprocessCompatible, false);
  do_check_eq(addon1.appDisabled, false);
  do_check_neq(addon2, null);
  do_check_eq(addon2.multiprocessCompatible, false);
  do_check_eq(addon2.appDisabled, false);

  await promiseShutdownManager();

  Services.prefs.setBoolPref(NON_MPC_PREF, false);
  gAppInfo.version = "2";

  // Simulate including a new blocklist with the new version by
  // flipping the pref below which causes the blocklist to be re-read.
  let blocklistPath = OS.Path.join(OS.Constants.Path.profileDir, "blocklist.xml");
  await OS.File.writeAtomic(blocklistPath, BLOCKLIST);
  let BLOCKLIST_PREF = "extensions.blocklist.enabled";
  Services.prefs.setBoolPref(BLOCKLIST_PREF, false);
  Services.prefs.setBoolPref(BLOCKLIST_PREF, true);

  await promiseStartupManager();

  // When we restart, one of the test addons should be blocklisted, and
  // one is incompatible.  Both are MPC=false but that should not trigger
  // the startup notification since flipping allow-non-mpc-extensions
  // won't re-enable either extension.
  const {STATE_BLOCKED} = Components.interfaces.nsIBlocklistService;
  [addon1, addon2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  do_check_neq(addon1, null);
  do_check_eq(addon1.appDisabled, true);
  do_check_eq(addon1.blocklistState, STATE_BLOCKED);
  do_check_neq(addon2, null);
  do_check_eq(addon2.appDisabled, true);
  do_check_eq(addon2.isCompatible, false);

  do_check_eq(AddonManager.nonMpcDisabled, false);

  addon1.uninstall();
  addon2.uninstall();

  Services.prefs.clearUserPref(NON_MPC_PREF);
});

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  startupManager();

  // Create and configure the HTTP server.
  gServer = new HttpServer();
  gServer.registerDirectory("/data/", gTmpD);
  gServer.start(-1);
  gPort = gServer.identity.primaryPort;

  run_next_test();
}

function end_test() {
  gServer.stop(do_test_finished);
}
