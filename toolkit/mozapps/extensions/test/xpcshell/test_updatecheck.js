/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that AddonUpdateChecker works correctly

Components.utils.import("resource://gre/modules/AddonUpdateChecker.jsm");


var channel = "default";
try {
  channel = Services.prefs.getCharPref("app.update.channel");
} catch (e) { }
if (channel != "aurora" && channel != "beta" && channel != "release")
  var version = "nightly";
else
  version = Services.appinfo.version.replace(/^([^\.]+\.[0-9]+[a-z]*).*/gi, "$1");
const COMPATIBILITY_PREF = "extensions.checkCompatibility." + version;


do_load_httpd_js();
var testserver;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.start(4444);

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Test that a basic update check returns the expected available updates
function run_test_1() {
  AddonUpdateChecker.checkForUpdates("updatecheck1@tests.mozilla.org", "extension", null,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      check_test_1(updates);
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function check_test_1(updates) {
  do_check_eq(updates.length, 5);
  let update = AddonUpdateChecker.getNewestCompatibleUpdate(updates);
  do_check_neq(update, null);
  do_check_eq(update.version, 3);
  update = AddonUpdateChecker.getCompatibilityUpdate(updates, "2");
  do_check_neq(update, null);
  do_check_eq(update.version, 2);
  do_check_eq(update.targetApplications[0].minVersion, 1);
  do_check_eq(update.targetApplications[0].maxVersion, 2);

  run_test_2();
}

/*
 * Tests that the security checks are applied correctly
 *
 * Test     signature   updateHash  updateLink   expected
 *--------------------------------------------------------
 * 2        absent      absent      http         fail
 * 3        broken      absent      http         fail
 * 4        correct     absent      http         no update
 * 5        correct     sha1        http         update
 * 6        corrent     absent      https        update
 * 7        corrent     sha1        https        update
 * 8        corrent     md2         http         no update
 * 9        corrent     md2         https        update
 */

let updateKey = "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDK426erD/H3XtsjvaB5+PJqbhj" +
                "Zc9EDI5OCJS8R3FIObJ9ZHJK1TXeaE7JWqt9WUmBWTEFvwS+FI9vWu8058N9CHhD" +
                "NyeP6i4LuUYjTURnn7Yw/IgzyIJ2oKsYa32RuxAyteqAWqPT/J63wBixIeCxmysf" +
                "awB/zH4KaPiY3vnrzQIDAQAB";

function run_test_2() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_5@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_throw("Expected the update check to fail");
    },

    onUpdateCheckError: function(status) {
      run_test_3();
    }
  });
}

function run_test_3() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_7@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_throw("Expected the update check to fail");
    },

    onUpdateCheckError: function(status) {
      run_test_4();
    }
  });
}

function run_test_4() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_8@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 1);
      do_check_false("updateURL" in updates[0]);
      run_test_5();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_5() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_9@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 1);
      do_check_eq(updates[0].version, "2.0");
      do_check_true("updateURL" in updates[0]);
      run_test_6();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_6() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_10@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 1);
      do_check_eq(updates[0].version, "2.0");
      do_check_true("updateURL" in updates[0]);
      run_test_7();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_7() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_11@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 1);
      do_check_eq(updates[0].version, "2.0");
      do_check_true("updateURL" in updates[0]);
      run_test_8();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_8() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_12@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 1);
      do_check_false("updateURL" in updates[0]);
      run_test_9();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_9() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_13@tests.mozilla.org",
                                     "extension", updateKey,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 1);
      do_check_eq(updates[0].version, "2.0");
      do_check_true("updateURL" in updates[0]);
      run_test_10();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_10() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_14@tests.mozilla.org",
                                     "extension", null,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 0);
      run_test_11();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_11() {
  AddonUpdateChecker.checkForUpdates("test_bug378216_15@tests.mozilla.org",
                                     "extension", null,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_throw("Update check should have failed");
    },

    onUpdateCheckError: function(status) {
      do_check_eq(status, AddonUpdateChecker.ERROR_PARSE_ERROR);
      run_test_12();
    }
  });
}

function run_test_12() {
  AddonUpdateChecker.checkForUpdates("ignore-compat@tests.mozilla.org",
                                     "extension", null,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 3);
      let update = AddonUpdateChecker.getNewestCompatibleUpdate(updates,
                                                                null,
                                                                null,
                                                                true);
      do_check_neq(update, null);
      do_check_eq(update.version, 2);
      run_test_13();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_13() {
  AddonUpdateChecker.checkForUpdates("compat-override@tests.mozilla.org",
                                     "extension", null,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 3);
      let overrides = [{
        type: "incompatible",
        minVersion: 1,
        maxVersion: 2,
        appID: "xpcshell@tests.mozilla.org",
        appMinVersion: 0.1,
        appMaxVersion: 0.2
      }, {
        type: "incompatible",
        minVersion: 2,
        maxVersion: 2,
        appID: "xpcshell@tests.mozilla.org",
        appMinVersion: 1,
        appMaxVersion: 2
      }];
      let update = AddonUpdateChecker.getNewestCompatibleUpdate(updates,
                                                                null,
                                                                null,
                                                                true,
                                                                false,
                                                                overrides);
      do_check_neq(update, null);
      do_check_eq(update.version, 1);
      run_test_14();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}

function run_test_14() {
  AddonUpdateChecker.checkForUpdates("compat-strict-optin@tests.mozilla.org",
                                     "extension", null,
                                     "http://localhost:4444/data/test_updatecheck.rdf", {
    onUpdateCheckComplete: function(updates) {
      do_check_eq(updates.length, 1);
      let update = AddonUpdateChecker.getNewestCompatibleUpdate(updates,
                                                                null,
                                                                null,
                                                                true,
                                                                false);
      do_check_eq(update, null);
      end_test();
    },

    onUpdateCheckError: function(status) {
      do_throw("Update check failed with status " + status);
    }
  });
}
