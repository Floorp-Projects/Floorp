/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests compatibility overrides, for when strict compatibility checking is
// disabled. See bug 693906.


const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

Components.utils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);
gPort = gServer.identity.primaryPort;

const PORT            = gPort;
const BASE_URL        = "http://localhost:" + PORT;
const REQ_URL         = "/data.xml";

// register static file and mark it for interpolation
mapUrlToFile(REQ_URL, do_get_file("data/test_compatoverrides.xml"), gServer);

Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
Services.prefs.setCharPref(PREF_GETADDONS_BYIDS_PERFORMANCE,
                           BASE_URL + REQ_URL);


// Not hosted, no overrides
var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, no overrides
var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, matching override
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, matching override, wouldn't be compatible if strict checking is enabled
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 4",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0.1",
    maxVersion: "0.2"
  }]
};

// Hosted, app ID doesn't match in override
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, addon version range doesn't match in override
var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 6",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, app version range doesn't match in override
var addon7 = {
  id: "addon7@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 7",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Hosted, multiple overrides
var addon8 = {
  id: "addon8@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 8",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Not hosted, matching override
var addon9 = {
  id: "addon9@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 9",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Not hosted, override is of unsupported type (compatible)
var addon10 = {
  id: "addon10@tests.mozilla.org",
  version: "1.0",
  name: "Test addon 10",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};


const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(addon6, profileDir);
  writeInstallRDFForExtension(addon7, profileDir);
  writeInstallRDFForExtension(addon8, profileDir);
  writeInstallRDFForExtension(addon9, profileDir);
  writeInstallRDFForExtension(addon10, profileDir);

  startupManager();

  AddonManagerInternal.backgroundUpdateCheck().then(run_test_1);
}

function end_test() {
  gServer.stop(do_test_finished);
}

function check_compat_status(aCallback) {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "addon8@tests.mozilla.org",
                               "addon9@tests.mozilla.org",
                               "addon10@tests.mozilla.org"],
                              function([a1, a2, a3, a4, a5, a6, a7, a8, a9, a10]) {

    Assert.notEqual(a1, null);
    Assert.equal(a1.compatibilityOverrides, null);
    Assert.ok(a1.isCompatible);
    Assert.ok(!a1.appDisabled);

    Assert.notEqual(a2, null);
    Assert.equal(a2.compatibilityOverrides, null);
    Assert.ok(a2.isCompatible);
    Assert.ok(!a2.appDisabled);

    Assert.notEqual(a3, null);
    Assert.notEqual(a3.compatibilityOverrides, null);
    Assert.equal(a3.compatibilityOverrides.length, 1);
    Assert.ok(!a3.isCompatible);
    Assert.ok(a3.appDisabled);

    Assert.notEqual(a4, null);
    Assert.notEqual(a4.compatibilityOverrides, null);
    Assert.equal(a4.compatibilityOverrides.length, 1);
    Assert.ok(!a4.isCompatible);
    Assert.ok(a4.appDisabled);

    Assert.notEqual(a5, null);
    Assert.equal(a5.compatibilityOverrides, null);
    Assert.ok(a5.isCompatible);
    Assert.ok(!a5.appDisabled);

    Assert.notEqual(a6, null);
    Assert.notEqual(a6.compatibilityOverrides, null);
    Assert.equal(a6.compatibilityOverrides.length, 1);
    Assert.ok(a6.isCompatible);
    Assert.ok(!a6.appDisabled);

    Assert.notEqual(a7, null);
    Assert.notEqual(a7.compatibilityOverrides, null);
    Assert.equal(a7.compatibilityOverrides.length, 1);
    Assert.ok(a7.isCompatible);
    Assert.ok(!a7.appDisabled);

    Assert.notEqual(a8, null);
    Assert.notEqual(a8.compatibilityOverrides, null);
    Assert.equal(a8.compatibilityOverrides.length, 3);
    Assert.ok(!a8.isCompatible);
    Assert.ok(a8.appDisabled);

    Assert.notEqual(a9, null);
    Assert.notEqual(a9.compatibilityOverrides, null);
    Assert.equal(a9.compatibilityOverrides.length, 1);
    Assert.ok(!a9.isCompatible);
    Assert.ok(a9.appDisabled);

    Assert.notEqual(a10, null);
    Assert.equal(a10.compatibilityOverrides, null);
    Assert.ok(a10.isCompatible);
    Assert.ok(!a10.appDisabled);

    executeSoon(aCallback);
  });
}

function run_test_1() {
  info("Run test 1");
  check_compat_status(run_test_2);
}

function run_test_2() {
  info("Run test 2");
  restartManager();
  check_compat_status(end_test);
}
