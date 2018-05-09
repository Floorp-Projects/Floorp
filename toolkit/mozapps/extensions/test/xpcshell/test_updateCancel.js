/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test cancelling add-on update checks while in progress (bug 925389)

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Set up an HTTP server to respond to update requests
ChromeUtils.import("resource://testing-common/httpd.js");

const profileDir = gProfD.clone();
profileDir.append("extensions");

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
  let updated = PromiseUtils.defer();
  return {
    onUpdateAvailable(addon, install) {
      updated.reject("Should not have seen onUpdateAvailable notification");
    },

    onUpdateFinished(aAddon, aError) {
      info("onUpdateCheckFinished: " + aAddon.id + " " + aError);
      updated.resolve(aError);
    },
    promise: updated.promise
  };
}

// Set up the HTTP server so that we can control when it responds
var httpReceived = PromiseUtils.defer();
function dataHandler(aRequest, aResponse) {
  aResponse.processAsync();
  httpReceived.resolve([aRequest, aResponse]);
}
var testserver = new HttpServer();
testserver.registerDirectory("/addons/", do_get_file("addons"));
testserver.registerPathHandler("/data/test_update.json", dataHandler);
testserver.start(-1);
gPort = testserver.identity.primaryPort;

// Set up an add-on for update check
add_task(async function setup() {
  await promiseWriteInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    updateURL: "http://localhost:" + gPort + "/data/test_update.json",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);
});

add_task(async function cancel_during_check() {
  await promiseStartupManager();

  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);

  let listener = makeCancelListener();
  a1.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);

  // Wait for the http request to arrive
  let [/* request */, response] = await httpReceived.promise;

  // cancelUpdate returns true if there is an update check in progress
  Assert.ok(a1.cancelUpdate());

  let updateResult = await listener.promise;
  Assert.equal(AddonManager.UPDATE_STATUS_CANCELLED, updateResult);

  // Now complete the HTTP request
  let file = do_get_cwd();
  file.append("data");
  file.append("test_update.json");
  let data = new TextDecoder().decode(await OS.File.read(file.path));
  response.write(data);
  response.finish();

  // trying to cancel again should return false, i.e. nothing to cancel
  Assert.ok(!a1.cancelUpdate());

  await true;
});

// Test that update check is cancelled if the XPI provider shuts down while
// the update check is in progress
add_task(async function shutdown_during_check() {
  // Reset our HTTP listener
  httpReceived = PromiseUtils.defer();

  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);

  let listener = makeCancelListener();
  a1.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);

  // Wait for the http request to arrive
  let [/* request */, response] = await httpReceived.promise;

  await promiseShutdownManager();

  let updateResult = await listener.promise;
  Assert.equal(AddonManager.UPDATE_STATUS_CANCELLED, updateResult);

  // Now complete the HTTP request
  let file = do_get_cwd();
  file.append("data");
  file.append("test_update.json");
  let data = await loadFile(file.path);
  response.write(data);
  response.finish();

  await testserver.stop();
});
