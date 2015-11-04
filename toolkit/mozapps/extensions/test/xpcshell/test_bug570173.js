/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update check failures are propogated correctly

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

Components.utils.import("resource://testing-common/httpd.js");
var testserver;
const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = createHttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  gPort = testserver.identity.primaryPort;

  run_next_test();
}

// Verify that an update check returns the correct errors.
add_task(function* () {
  for (let manifestType of ["rdf", "json"]) {
    writeInstallRDFForExtension({
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      updateURL: `http://localhost:${gPort}/data/test_missing.${manifestType}`,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 1",
      bootstrap: "true",
    }, profileDir);

    yield promiseRestartManager();

    let addon = yield promiseAddonByID("addon1@tests.mozilla.org");

    ok(addon);
    ok(addon.updateURL.endsWith(manifestType));
    equal(addon.version, "1.0");

    // We're expecting an error, so resolve when the promise is rejected.
    let update = yield promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED)
      .catch(Promise.resolve);

    ok(!update.compatibilityUpdate, "not expecting a compatibility update");
    ok(!update.updateAvailable, "not expecting a compatibility update");

    equal(update.error, AddonManager.UPDATE_STATUS_DOWNLOAD_ERROR);

    addon.uninstall();
  }
});
