/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that deleting the database from the profile doesn't break
// anything

const profileDir = gProfD.clone();
profileDir.append("extensions");

// getting an unused port
Components.utils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);
gPort = gServer.identity.primaryPort;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

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

  startupManager();

  do_test_pending();

  run_test_1();
}

function end_test() {
  gServer.stop(do_test_finished);
}

function run_test_1() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.version, "1.0");

    shutdownManager();

    gExtensionsJSON.remove(true);

    executeSoon(check_test_1);
  }));
}

function check_test_1() {
  startupManager(false);

  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.version, "1.0");

    // due to delayed write, the file may not exist until
    // after shutdown
    shutdownManager();
    Assert.ok(gExtensionsJSON.exists());
    Assert.ok(gExtensionsJSON.fileSize > 0);

    end_test();
  }));
}
