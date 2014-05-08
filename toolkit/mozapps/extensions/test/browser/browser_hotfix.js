/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const PREF_EM_HOTFIX_ID                = "extensions.hotfix.id";
const PREF_EM_HOTFIX_LASTVERSION       = "extensions.hotfix.lastVersion";
const PREF_EM_HOTFIX_URL               = "extensions.hotfix.url";
const PREF_EM_HOTFIX_CERTS             = "extensions.hotfix.certs.";
const PREF_EM_CERT_CHECKATTRIBUTES     = "extensions.hotfix.cert.checkAttributes";

const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const PREF_UPDATE_REQUIREBUILTINCERTS  = "extensions.update.requireBuiltInCerts";

const PREF_APP_UPDATE_ENABLED          = "app.update.enabled";

const HOTFIX_ID = "hotfix@tests.mozilla.org";

var gNextTest;

var SuccessfulInstallListener = {
  onDownloadCancelled: function(aInstall) {
    ok(false, "Should not have seen the download cancelled");
    is(aInstall.addon.id, HOTFIX_ID, "Should have seen the right add-on");

    AddonManager.removeInstallListener(this);
    gNextTest();
  },

  onInstallEnded: function(aInstall) {
    ok(true, "Should have seen the install complete");
    is(aInstall.addon.id, HOTFIX_ID, "Should have installed the right add-on");

    AddonManager.removeInstallListener(this);
    aInstall.addon.uninstall();
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_LASTVERSION);
    gNextTest();
  }
}

var FailedInstallListener = {
  onDownloadCancelled: function(aInstall) {
    ok(true, "Should have seen the download cancelled");
    is(aInstall.addon.id, HOTFIX_ID, "Should have seen the right add-on");

    AddonManager.removeInstallListener(this);
    gNextTest();
  },

  onInstallEnded: function(aInstall) {
    ok(false, "Should not have seen the install complete");
    is(aInstall.addon.id, HOTFIX_ID, "Should have installed the right add-on");

    AddonManager.removeInstallListener(this);
    aInstall.addon.uninstall();
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_LASTVERSION);
    gNextTest();
  }
}

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, true);
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);
  Services.prefs.setBoolPref(PREF_UPDATE_REQUIREBUILTINCERTS, false);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_ID, HOTFIX_ID);
  var oldURL = Services.prefs.getCharPref(PREF_EM_HOTFIX_URL);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_URL, TESTROOT + "signed_hotfix.rdf");

  registerCleanupFunction(function() {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, false);
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_ID);
    Services.prefs.setCharPref(PREF_EM_HOTFIX_URL, oldURL);
    Services.prefs.clearUserPref(PREF_INSTALL_REQUIREBUILTINCERTS);
    Services.prefs.clearUserPref(PREF_UPDATE_REQUIREBUILTINCERTS);

    Services.prefs.clearUserPref(PREF_EM_CERT_CHECKATTRIBUTES);
    var prefs = Services.prefs.getChildList(PREF_EM_HOTFIX_CERTS);
    prefs.forEach(Services.prefs.clearUserPref);
  });

  run_next_test();
}

function end_test() {
  finish();
}

add_test(function check_no_cert_checks() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, false);
  AddonManager.addInstallListener(SuccessfulInstallListener);

  gNextTest = run_next_test;

  AddonManagerPrivate.backgroundUpdateCheck();
});

add_test(function check_wrong_cert_fingerprint() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "foo");

  AddonManager.addInstallListener(FailedInstallListener);

  gNextTest = function() {
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");

    run_next_test();
  };

  AddonManagerPrivate.backgroundUpdateCheck();
});

add_test(function check_right_cert_fingerprint() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");

  AddonManager.addInstallListener(SuccessfulInstallListener);

  gNextTest = function() {
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");

    run_next_test();
  };

  AddonManagerPrivate.backgroundUpdateCheck();
});

add_test(function check_multi_cert_fingerprint_1() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint", "foo");

  AddonManager.addInstallListener(SuccessfulInstallListener);

  gNextTest = function() {
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint");

    run_next_test();
  };

  AddonManagerPrivate.backgroundUpdateCheck();
});

add_test(function check_multi_cert_fingerprint_2() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "foo");
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");

  AddonManager.addInstallListener(SuccessfulInstallListener);

  gNextTest = function() {
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint");

    run_next_test();
  };

  AddonManagerPrivate.backgroundUpdateCheck();
});

add_test(function check_no_cert_no_checks() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, false);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_URL, TESTROOT + "unsigned_hotfix.rdf");

  AddonManager.addInstallListener(SuccessfulInstallListener);

  gNextTest = run_next_test;

  AddonManagerPrivate.backgroundUpdateCheck();
});

add_test(function check_no_cert_cert_fingerprint_check() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");

  AddonManager.addInstallListener(FailedInstallListener);

  gNextTest = function() {
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");

    run_next_test();
  };

  AddonManagerPrivate.backgroundUpdateCheck();
});
