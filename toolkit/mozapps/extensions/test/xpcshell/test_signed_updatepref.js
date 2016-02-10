// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
gUseRealCertChecks = true;

const DATA = "data/signing_checks/";
const ID = "test@tests.mozilla.org";

Components.utils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start();

gServer.registerPathHandler("/update.rdf", function(request, response) {
  let updateData = {};
  updateData[ID] = [{
    version: "2.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "4",
      maxVersion: "6"
    }]
  }];

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(createUpdateRDF(updateData));
});

const SERVER = "127.0.0.1:" + gServer.identity.primaryPort;
Services.prefs.setCharPref("extensions.update.background.url", "http://" + SERVER + "/update.rdf");

function verifySignatures() {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      Services.obs.removeObserver(observer, "xpi-signature-changed");
      resolve(JSON.parse(data));
    }
    Services.obs.addObserver(observer, "xpi-signature-changed", false);

    do_print("Verifying signatures");
    let XPIscope = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm");
    XPIscope.XPIProvider.verifySignatures();
  });
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");

  // Start and stop the manager to initialise everything in the profile before
  // actual testing
  startupManager();
  shutdownManager();

  run_next_test();
}

// Updating the pref without changing the app version won't disable add-ons
// immediately but will after a signing check
add_task(function*() {
  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, false);
  startupManager();

  // Install the signed add-on
  yield promiseInstallAllFiles([do_get_file(DATA + "unsigned_bootstrap_2.xpi")]);

  let addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  yield promiseShutdownManager();

  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);

  startupManager();

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  // Update checks shouldn't affect the add-on
  yield AddonManagerInternal.backgroundUpdateCheck();
  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  let changes = yield verifySignatures();

  do_check_eq(changes.disabled.length, 1);
  do_check_eq(changes.disabled[0], ID);

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_true(addon.appDisabled);
  do_check_false(addon.isActive);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  addon.uninstall();

  yield promiseShutdownManager();
});

// Updating the pref with changing the app version will disable add-ons
// immediately
add_task(function*() {
  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, false);
  startupManager();

  // Install the signed add-on
  yield promiseInstallAllFiles([do_get_file(DATA + "unsigned_bootstrap_2.xpi")]);

  let addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  yield promiseShutdownManager();

  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);
  gAppInfo.version = 5.0
  startupManager(true);

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_true(addon.appDisabled);
  do_check_false(addon.isActive);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  addon.uninstall();

  yield promiseShutdownManager();
});
