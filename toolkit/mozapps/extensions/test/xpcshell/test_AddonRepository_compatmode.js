/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that AddonRepository correctly fills in the
// %COMPATIBILITY_MODE% token in the Search API URL.

const PREF_GETADDONS_GETSEARCHRESULTS    = "extensions.getAddons.search.url";

Components.utils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);
gPort = gServer.identity.primaryPort;

// register static files with server and interpolate port numbers in them
mapFile("/data/test_AddonRepository_compatmode_ignore.xml", gServer);
mapFile("/data/test_AddonRepository_compatmode_normal.xml", gServer);
mapFile("/data/test_AddonRepository_compatmode_strict.xml", gServer);

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS,
                             "http://localhost:" + gPort + "/data/test_AddonRepository_compatmode_%COMPATIBILITY_MODE%.xml");
  startupManager();
  run_test_1();
}

function end_test() {
  gServer.stop(do_test_finished);
}

// Strict compatibility checking disabled.
function run_test_1() {
  do_print("Testing with strict compatibility checking disabled");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

  AddonRepository.searchAddons("test", 6, {
    searchSucceeded(aAddons) {
      do_check_neq(aAddons, null);
      do_check_eq(aAddons.length, 1);
      do_check_eq(aAddons[0].id, "compatmode-normal@tests.mozilla.org");

      run_test_2();
    },
    searchFailed() {
      do_throw("Search should not have failed");
    }
  });
}

// Strict compatibility checking enabled.
function run_test_2() {
  do_print("Testing with strict compatibility checking enabled");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);

  AddonRepository.searchAddons("test", 6, {
    searchSucceeded(aAddons) {
      do_check_neq(aAddons, null);
      do_check_eq(aAddons.length, 1);
      do_check_eq(aAddons[0].id, "compatmode-strict@tests.mozilla.org");

      run_test_3();
    },
    searchFailed() {
      do_throw("Search should not have failed");
    }
  });
}

// Compatibility checking disabled.
function run_test_3() {
  do_print("Testing with all compatibility checking disabled");
  AddonManager.checkCompatibility = false;

  AddonRepository.searchAddons("test", 6, {
    searchSucceeded(aAddons) {
      do_check_neq(aAddons, null);
      do_check_eq(aAddons.length, 1);
      do_check_eq(aAddons[0].id, "compatmode-ignore@tests.mozilla.org");

      end_test();
    },
    searchFailed() {
      do_throw("Search should not have failed");
    }
  });
}
