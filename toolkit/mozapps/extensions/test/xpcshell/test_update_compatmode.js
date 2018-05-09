/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update check correctly fills in the
// %COMPATIBILITY_MODE% token in the update URL.


// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));
testserver.registerDirectory("/addons/", do_get_file("addons"));

const profileDir = gProfD.clone();
profileDir.append("extensions");

async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseWriteInstallRDFForExtension({
    id: "compatmode-normal@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    updateURL: "http://example.com/data/test_updatecompatmode_%COMPATIBILITY_MODE%.json",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - normal"
  }, profileDir);

  await promiseWriteInstallRDFForExtension({
    id: "compatmode-strict@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    updateURL: "http://example.com/data/test_updatecompatmode_%COMPATIBILITY_MODE%.json",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - strict"
  }, profileDir);

  await promiseWriteInstallRDFForExtension({
    id: "compatmode-strict-optin@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    updateURL: "http://example.com/data/test_updatecompatmode_%COMPATIBILITY_MODE%.json",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - strict opt-in",
    strictCompatibility: true
  }, profileDir);

  await promiseWriteInstallRDFForExtension({
    id: "compatmode-ignore@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    updateURL: "http://example.com/data/test_updatecompatmode_%COMPATIBILITY_MODE%.json",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - ignore",
  }, profileDir);

  await promiseStartupManager();
  run_test_1();
}

function end_test() {
  do_test_finished();
}


// Strict compatibility checking disabled.
async function run_test_1() {
  info("Testing with strict compatibility checking disabled");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  let addon = await AddonManager.getAddonByID("compatmode-normal@tests.mozilla.org");
  Assert.notEqual(addon, null);
  addon.findUpdates({
    onCompatibilityUpdateAvailable() {
      do_throw("Should have not have seen compatibility information");
    },

    onNoUpdateAvailable() {
      do_throw("Should have seen an available update");
    },

    onUpdateAvailable(unused, install) {
      Assert.equal(install.version, "2.0");
    },

    onUpdateFinished() {
      run_test_2();
    }
  }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
}

// Strict compatibility checking enabled.
async function run_test_2() {
  info("Testing with strict compatibility checking enabled");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  let addon = await AddonManager.getAddonByID("compatmode-strict@tests.mozilla.org");
  Assert.notEqual(addon, null);
  addon.findUpdates({
    onCompatibilityUpdateAvailable() {
      do_throw("Should have not have seen compatibility information");
    },

    onNoUpdateAvailable() {
      do_throw("Should have seen an available update");
    },

    onUpdateAvailable(unused, install) {
      Assert.equal(install.version, "2.0");
    },

    onUpdateFinished() {
      run_test_3();
    }
  }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
}

// Strict compatibility checking opt-in.
async function run_test_3() {
  info("Testing with strict compatibility disabled, but addon opt-in");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  let addon = await AddonManager.getAddonByID("compatmode-strict-optin@tests.mozilla.org");
  Assert.notEqual(addon, null);
  addon.findUpdates({
    onCompatibilityUpdateAvailable() {
      do_throw("Should have not have seen compatibility information");
    },

    onUpdateAvailable() {
      do_throw("Should not have seen an available update");
    },

    onUpdateFinished() {
      run_test_4();
    }
  }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
}

// Compatibility checking disabled.
async function run_test_4() {
  info("Testing with all compatibility checking disabled");
  AddonManager.checkCompatibility = false;
  let addon = await AddonManager.getAddonByID("compatmode-ignore@tests.mozilla.org");
  Assert.notEqual(addon, null);
  addon.findUpdates({
    onCompatibilityUpdateAvailable() {
      do_throw("Should have not have seen compatibility information");
    },

    onNoUpdateAvailable() {
      do_throw("Should have seen an available update");
    },

    onUpdateAvailable(unused, install) {
      Assert.equal(install.version, "2.0");
    },

    onUpdateFinished() {
      end_test();
    }
  }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
}
