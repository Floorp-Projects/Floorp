/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test cancelling add-on update checks while in progress (bug 925389)

Components.utils.import("resource://gre/modules/Promise.jsm");

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Set up an HTTP server to respond to update requests
Components.utils.import("resource://testing-common/httpd.js");

const profileDir = gProfD.clone();
profileDir.append("extensions");


function run_test() {
  // Kick off the task-based tests...
  run_next_test();
}

// Install one extension
// Start download of update check (but delay HTTP response)
// Cancel update check
//  - ensure we get cancel notification
// complete HTTP response
//  - ensure no callbacks after cancel
//  - ensure update is gone

// Create an addon update listener containing a promise
// that resolves when the update is cancelled
function makeCancelListener() {
  let updated = Promise.defer();
  return {
    onUpdateAvailable: function(addon, install) {
      updated.reject("Should not have seen onUpdateAvailable notification");
    },

    onUpdateFinished: function(aAddon, aError) {
      do_print("onUpdateCheckFinished: " + aAddon.id + " " + aError);
      updated.resolve(aError);
    },
    promise: updated.promise
  };
}

// Set up the HTTP server so that we can control when it responds
var httpReceived = Promise.defer();
function dataHandler(aRequest, aResponse) {
  asyncResponse = aResponse;
  aResponse.processAsync();
  httpReceived.resolve([aRequest, aResponse]);
}
var testserver = new HttpServer();
testserver.registerDirectory("/addons/", do_get_file("addons"));
testserver.registerPathHandler("/data/test_update.rdf", dataHandler);
testserver.start(-1);
gPort = testserver.identity.primaryPort;

// Set up an add-on for update check
writeInstallRDFForExtension({
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  updateURL: "http://localhost:" + gPort + "/data/test_update.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }],
  name: "Test Addon 1",
}, profileDir);

add_task(function cancel_during_check() {
  startupManager();

  let a1 = yield promiseAddonByID("addon1@tests.mozilla.org");
  do_check_neq(a1, null);

  let listener = makeCancelListener();
  a1.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);

  // Wait for the http request to arrive
  let [request, response] = yield httpReceived.promise;

  // cancelUpdate returns true if there is an update check in progress
  do_check_true(a1.cancelUpdate());

  let updateResult = yield listener.promise;
  do_check_eq(AddonManager.UPDATE_STATUS_CANCELLED, updateResult);

  // Now complete the HTTP request
  let file = do_get_cwd();
  file.append("data");
  file.append("test_update.rdf");
  let data = loadFile(file);
  response.write(data);
  response.finish();

  // trying to cancel again should return false, i.e. nothing to cancel
  do_check_false(a1.cancelUpdate());

  yield true;
});

// Test that update check is cancelled if the XPI provider shuts down while
// the update check is in progress
add_task(function shutdown_during_check() {
  // Reset our HTTP listener
  httpReceived = Promise.defer();

  let a1 = yield promiseAddonByID("addon1@tests.mozilla.org");
  do_check_neq(a1, null);

  let listener = makeCancelListener();
  a1.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);

  // Wait for the http request to arrive
  let [request, response] = yield httpReceived.promise;

  shutdownManager();

  let updateResult = yield listener.promise;
  do_check_eq(AddonManager.UPDATE_STATUS_CANCELLED, updateResult);

  // Now complete the HTTP request
  let file = do_get_cwd();
  file.append("data");
  file.append("test_update.rdf");
  let data = loadFile(file);
  response.write(data);
  response.finish();

  yield testserver.stop(Promise.defer().resolve);
});
