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
const PREF_APP_UPDATE_URL              = "app.update.url";

const HOTFIX_ID = "hotfix@tests.mozilla.org";

/*
 * Register an addon install listener and return a promise that:
 *  resolves with the AddonInstall object if the install succeeds
 *  rejects with the AddonInstall if the install fails
 */
function promiseInstallListener() {
  return new Promise((resolve, reject) => {
    let listener = {
      onInstallEnded: ai => {
        AddonManager.removeInstallListener(listener);
        resolve(ai);
      },
      onDownloadCancelled: ai => {
        AddonManager.removeInstallListener(listener);
        reject(ai);
      }
    };
    AddonManager.addInstallListener(listener);
  });
}

function promiseSuccessfulInstall() {
  return promiseInstallListener().then(
    aInstall => {
      ok(true, "Should have seen the install complete");
      is(aInstall.addon.id, HOTFIX_ID, "Should have installed the right add-on");
      aInstall.addon.uninstall();
      Services.prefs.clearUserPref(PREF_EM_HOTFIX_LASTVERSION);
    },
    aInstall => {
      ok(false, "Should not have seen the download cancelled");
      is(aInstall.addon.id, HOTFIX_ID, "Should have seen the right add-on");
    });
}

function promiseFailedInstall() {
  return promiseInstallListener().then(
    aInstall => {
      ok(false, "Should not have seen the install complete");
      is(aInstall.addon.id, HOTFIX_ID, "Should have installed the right add-on");
      aInstall.addon.uninstall();
      Services.prefs.clearUserPref(PREF_EM_HOTFIX_LASTVERSION);
    },
    aInstall => {
      ok(true, "Should have seen the download cancelled");
      is(aInstall.addon.id, HOTFIX_ID, "Should have seen the right add-on");
    });
}

add_task(function setup() {
  var oldAusUrl = Services.prefs.getDefaultBranch(null).getCharPref(PREF_APP_UPDATE_URL);
  Services.prefs.getDefaultBranch(null).setCharPref(PREF_APP_UPDATE_URL, TESTROOT + "ausdummy.xml");
  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, true);
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);
  Services.prefs.setBoolPref(PREF_UPDATE_REQUIREBUILTINCERTS, false);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_ID, HOTFIX_ID);
  var oldURL = Services.prefs.getCharPref(PREF_EM_HOTFIX_URL);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_URL, TESTROOT + "signed_hotfix.rdf");

  registerCleanupFunction(function() {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, false);
    Services.prefs.getDefaultBranch(null).setCharPref(PREF_APP_UPDATE_URL, oldAusUrl);
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_ID);
    Services.prefs.setCharPref(PREF_EM_HOTFIX_URL, oldURL);
    Services.prefs.clearUserPref(PREF_INSTALL_REQUIREBUILTINCERTS);
    Services.prefs.clearUserPref(PREF_UPDATE_REQUIREBUILTINCERTS);

    Services.prefs.clearUserPref(PREF_EM_CERT_CHECKATTRIBUTES);
    var prefs = Services.prefs.getChildList(PREF_EM_HOTFIX_CERTS);
    prefs.forEach(Services.prefs.clearUserPref);
  });
});

add_task(function* check_no_cert_checks() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, false);
  yield Promise.all([
    promiseSuccessfulInstall(),
    AddonManagerPrivate.backgroundUpdateCheck()
  ]);
});

add_task(function* check_wrong_cert_fingerprint() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "foo");

  yield Promise.all([
    promiseFailedInstall(),
    AddonManagerPrivate.backgroundUpdateCheck()
  ]);
  Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");
});

add_task(function* check_right_cert_fingerprint() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");

  yield Promise.all([
    promiseSuccessfulInstall(),
    AddonManagerPrivate.backgroundUpdateCheck()
  ]);

  Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");
});

add_task(function* check_multi_cert_fingerprint_1() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint", "foo");

  yield Promise.all([
    promiseSuccessfulInstall(),
    AddonManagerPrivate.backgroundUpdateCheck()
  ]);

  Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");
  Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint");
});

add_task(function* check_multi_cert_fingerprint_2() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "foo");
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");

  yield Promise.all([
    promiseSuccessfulInstall(),
    AddonManagerPrivate.backgroundUpdateCheck()
  ]);

  Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");
  Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "2.sha1Fingerprint");
});

add_task(function* check_no_cert_no_checks() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, false);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_URL, TESTROOT + "unsigned_hotfix.rdf");

  yield Promise.all([
    promiseSuccessfulInstall(),
    AddonManagerPrivate.backgroundUpdateCheck()
  ]);
});

add_task(function* check_no_cert_cert_fingerprint_check() {
  Services.prefs.setBoolPref(PREF_EM_CERT_CHECKATTRIBUTES, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint", "3E:B9:4E:07:12:FE:3C:01:41:46:13:46:FC:84:52:1A:8C:BE:1D:A2");

  yield Promise.all([
    promiseFailedInstall(),
    AddonManagerPrivate.backgroundUpdateCheck()
  ]);

  Services.prefs.clearUserPref(PREF_EM_HOTFIX_CERTS + "1.sha1Fingerprint");
});
