Components.utils.import("resource://testing-common/httpd.js");
var gServer;

const profileDir = gProfD.clone();
profileDir.append("extensions");

const NON_MPC_PREF = "extensions.allow-non-mpc-extensions";

Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

function build_test(multiprocessCompatible, bootstrap, updateMultiprocessCompatible) {
  return async function() {
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
    };

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
    let install = await AddonManager.getInstallForFile(xpifile);
    do_check_eq(install.addon.multiprocessCompatible, !!multiprocessCompatible);
    do_check_eq(install.addon.mpcOptedOut, multiprocessCompatible === false);
    await promiseCompleteAllInstalls([install]);

    if (!bootstrap) {
      await promiseRestartManager();
      do_check_true(isExtensionInAddonsList(profileDir, addonData.id));
      do_check_eq(isItemMarkedMPIncompatible(addonData.id), !multiprocessCompatible);
    }

    let addon = await promiseAddonByID(addonData.id);
    do_check_neq(addon, null);
    do_check_eq(addon.multiprocessCompatible, !!multiprocessCompatible);
    do_check_eq(addon.mpcOptedOut, multiprocessCompatible === false);

    await promiseFindAddonUpdates(addon);

    // Should have applied the compatibility change
    do_check_eq(addon.multiprocessCompatible, !!expectedMPC);
    await promiseRestartManager();

    addon = await promiseAddonByID(addonData.id);
    // Should have persisted the compatibility change
    do_check_eq(addon.multiprocessCompatible, !!expectedMPC);
    if (!bootstrap) {
      do_check_true(isExtensionInAddonsList(profileDir, addonData.id));
      do_check_eq(isItemMarkedMPIncompatible(addonData.id), !multiprocessCompatible);
    }

    addon.uninstall();
    await promiseRestartManager();

    gServer.registerPathHandler("/updaterdf", null);
  };
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
  };

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
    let newValue = !(initialAllow === true);
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
