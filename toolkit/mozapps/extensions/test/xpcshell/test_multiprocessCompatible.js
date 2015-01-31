Components.utils.import("resource://testing-common/httpd.js");
var gServer;

const profileDir = gProfD.clone();
profileDir.append("extensions");

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
    let install = yield new Promise(resolve => AddonManager.getInstallForFile(xpifile, resolve));
    do_check_eq(install.addon.multiprocessCompatible, multiprocessCompatible);
    yield promiseCompleteAllInstalls([install]);

    if (!bootstrap) {
      yield promiseRestartManager();
      do_check_true(isExtensionInAddonsList(profileDir, addonData.id));
      do_check_eq(isItemMarkedMPIncompatible(addonData.id), !multiprocessCompatible);
    }

    let addon = yield promiseAddonByID(addonData.id);
    do_check_neq(addon, null);
    do_check_eq(addon.multiprocessCompatible, multiprocessCompatible);

    yield promiseFindAddonUpdates(addon);

    // Should have applied the compatibility change
    do_check_eq(addon.multiprocessCompatible, expectedMPC);
    yield promiseRestartManager();

    addon = yield promiseAddonByID(addonData.id);
    // Should have persisted the compatibility change
    do_check_eq(addon.multiprocessCompatible, expectedMPC);
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
  for (let multiprocessCompatible of [false, true]) {
    for (let updateMultiprocessCompatible of [undefined, false, true]) {
      add_task(build_test(multiprocessCompatible, bootstrap, updateMultiprocessCompatible));
    }
  }
}

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
