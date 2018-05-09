/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that background updates & notifications work as expected

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
const profileDir = gProfD.clone();
profileDir.append("extensions");

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.registerDirectory("/data/", do_get_file("data"));

  await promiseStartupManager();

  do_test_pending();
  run_test_1();
});

function end_test() {
  do_test_finished();
}

// Verify that with no add-ons installed the background update notifications get
// called
async function run_test_1() {
  let aAddons = await AddonManager.getAddonsByTypes(["extension", "theme", "locale"]);
  Assert.equal(aAddons.length, 1);

  Services.obs.addObserver(function observer() {
    Services.obs.removeObserver(observer, "addons-background-update-complete");

    executeSoon(run_test_2);
  }, "addons-background-update-complete");

  // Trigger the background update timer handler
  gInternalManager.notify(null);
}

// Verify that with two add-ons installed both of which claim to have updates
// available we get the notification after both updates attempted to start
async function run_test_2() {
  await promiseWriteInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://example.com/data/test_backgroundupdate.json",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);

  await promiseWriteInstallRDFForExtension({
    id: "addon2@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://example.com/data/test_backgroundupdate.json",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 2",
  }, profileDir);

  await promiseWriteInstallRDFForExtension({
    id: "addon3@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 3",
  }, profileDir);

  // Disable rcwn to make cache behavior deterministic.
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  // Background update uses a different pref, if set
  Services.prefs.setCharPref("extensions.update.background.url",
                             "http://example.com/data/test_backgroundupdate.json");

  await promiseRestartManager();

  let installCount = 0;
  let completeCount = 0;
  let sawCompleteNotification = false;

  Services.obs.addObserver(function observer() {
    Services.obs.removeObserver(observer, "addons-background-update-complete");

    Assert.equal(installCount, 3);
    sawCompleteNotification = true;
  }, "addons-background-update-complete");

  AddonManager.addInstallListener({
    onNewInstall(aInstall) {
      installCount++;
    },

    onDownloadFailed(aInstall) {
      completeCount++;
      if (completeCount == 3) {
        Assert.ok(sawCompleteNotification);
        end_test();
      }
    }
  });

  // Trigger the background update timer handler
  gInternalManager.notify(null);
}
