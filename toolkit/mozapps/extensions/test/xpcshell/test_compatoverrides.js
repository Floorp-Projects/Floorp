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
const DEFAULT_URL     = "about:blank";
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

    do_check_neq(a1, null);
    do_check_eq(a1.compatibilityOverrides, null);
    do_check_true(a1.isCompatible);
    do_check_false(a1.appDisabled);

    do_check_neq(a2, null);
    do_check_eq(a2.compatibilityOverrides, null);
    do_check_true(a2.isCompatible);
    do_check_false(a2.appDisabled);

    do_check_neq(a3, null);
    do_check_neq(a3.compatibilityOverrides, null);
    do_check_eq(a3.compatibilityOverrides.length, 1);
    do_check_false(a3.isCompatible);
    do_check_true(a3.appDisabled);

    do_check_neq(a4, null);
    do_check_neq(a4.compatibilityOverrides, null);
    do_check_eq(a4.compatibilityOverrides.length, 1);
    do_check_false(a4.isCompatible);
    do_check_true(a4.appDisabled);

    do_check_neq(a5, null);
    do_check_eq(a5.compatibilityOverrides, null);
    do_check_true(a5.isCompatible);
    do_check_false(a5.appDisabled);

    do_check_neq(a6, null);
    do_check_neq(a6.compatibilityOverrides, null);
    do_check_eq(a6.compatibilityOverrides.length, 1);
    do_check_true(a6.isCompatible);
    do_check_false(a6.appDisabled);

    do_check_neq(a7, null);
    do_check_neq(a7.compatibilityOverrides, null);
    do_check_eq(a7.compatibilityOverrides.length, 1);
    do_check_true(a7.isCompatible);
    do_check_false(a7.appDisabled);

    do_check_neq(a8, null);
    do_check_neq(a8.compatibilityOverrides, null);
    do_check_eq(a8.compatibilityOverrides.length, 3);
    do_check_false(a8.isCompatible);
    do_check_true(a8.appDisabled);

    do_check_neq(a9, null);
    do_check_neq(a9.compatibilityOverrides, null);
    do_check_eq(a9.compatibilityOverrides.length, 1);
    do_check_false(a9.isCompatible);
    do_check_true(a9.appDisabled);

    do_check_neq(a10, null);
    do_check_eq(a10.compatibilityOverrides, null);
    do_check_true(a10.isCompatible);
    do_check_false(a10.appDisabled);

    do_execute_soon(aCallback);
  });
}

function run_test_1() {
  do_print("Run test 1");
  check_compat_status(run_test_2);
}

function run_test_2() {
  do_print("Run test 2");
  restartManager();
  check_compat_status(end_test);
}
