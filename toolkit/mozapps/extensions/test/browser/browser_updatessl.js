/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var tempScope = {};
Components.utils.import("resource://gre/modules/addons/AddonUpdateChecker.jsm", tempScope);
var AddonUpdateChecker = tempScope.AddonUpdateChecker;

const updaterdf = RELATIVE_DIR + "browser_updatessl.rdf";
const redirect = RELATIVE_DIR + "redirect.sjs?";
const SUCCESS = 0;
const DOWNLOAD_ERROR = AddonUpdateChecker.ERROR_DOWNLOAD_ERROR;

const HTTP = "http://example.com/";
const HTTPS = "https://example.com/";
const NOCERT = "https://nocert.example.com/";
const SELFSIGNED = "https://self-signed.example.com/";
const UNTRUSTED = "https://untrusted.example.com/";
const EXPIRED = "https://expired.example.com/";

const PREF_UPDATE_REQUIREBUILTINCERTS = "extensions.update.requireBuiltInCerts";

var gTests = [];
var gStart = 0;
var gLast = 0;

var HTTPObserver = {
  observeActivity(aChannel, aType, aSubtype, aTimestamp, aSizeData,
                            aStringData) {
    aChannel.QueryInterface(Ci.nsIChannel);

    dump("*** HTTP Activity 0x" + aType.toString(16) + " 0x" + aSubtype.toString(16) +
         " " + aChannel.URI.spec + "\n");
  }
};

function test() {
  gStart = Date.now();
  requestLongerTimeout(4);
  waitForExplicitFinish();

  let observerService = Cc["@mozilla.org/network/http-activity-distributor;1"].
                        getService(Ci.nsIHttpActivityDistributor);
  observerService.addObserver(HTTPObserver);

  registerCleanupFunction(function() {
    observerService.removeObserver(HTTPObserver);
  });

  run_next_test();
}

function end_test() {
  Services.prefs.clearUserPref(PREF_UPDATE_REQUIREBUILTINCERTS);

  var cos = Cc["@mozilla.org/security/certoverride;1"].
            getService(Ci.nsICertOverrideService);
  cos.clearValidityOverride("nocert.example.com", -1);
  cos.clearValidityOverride("self-signed.example.com", -1);
  cos.clearValidityOverride("untrusted.example.com", -1);
  cos.clearValidityOverride("expired.example.com", -1);

  info("All tests completed in " + (Date.now() - gStart) + "ms");
  finish();
}

function add_update_test(mainURL, redirectURL, expectedStatus) {
  gTests.push([mainURL, redirectURL, expectedStatus]);
}

function run_update_tests(callback) {
  function run_next_update_test() {
    if (gTests.length == 0) {
      callback();
      return;
    }
    gLast = Date.now();

    let [mainURL, redirectURL, expectedStatus] = gTests.shift();
    if (redirectURL) {
      var url = mainURL + redirect + redirectURL + updaterdf;
      var message = "Should have seen the right result for an update check redirected from " +
                    mainURL + " to " + redirectURL;
    }
    else {
      url = mainURL + updaterdf;
      message = "Should have seen the right result for an update check from " +
                mainURL;
    }

    AddonUpdateChecker.checkForUpdates("addon1@tests.mozilla.org",
                                       null, url, {
      onUpdateCheckComplete(updates) {
        is(updates.length, 1, "Should be the right number of results");
        is(SUCCESS, expectedStatus, message);
        info("Update test ran in " + (Date.now() - gLast) + "ms");
        run_next_update_test();
      },

      onUpdateCheckError(status) {
        is(status, expectedStatus, message);
        info("Update test ran in " + (Date.now() - gLast) + "ms");
        run_next_update_test();
      }
    });
  }

  run_next_update_test();
}

// Add overrides for the bad certificates
function addCertOverrides() {
  addCertOverride("nocert.example.com", Ci.nsICertOverrideService.ERROR_MISMATCH);
  addCertOverride("self-signed.example.com", Ci.nsICertOverrideService.ERROR_UNTRUSTED);
  addCertOverride("untrusted.example.com", Ci.nsICertOverrideService.ERROR_UNTRUSTED);
  addCertOverride("expired.example.com", Ci.nsICertOverrideService.ERROR_TIME);
}

// Runs tests with built-in certificates required and no certificate exceptions.
add_test(function() {
  // Tests that a simple update.rdf retrieval works as expected.
  add_update_test(HTTP,       null,       SUCCESS);
  add_update_test(HTTPS,      null,       DOWNLOAD_ERROR);
  add_update_test(NOCERT,     null,       DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, null,       DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  null,       DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    null,       DOWNLOAD_ERROR);

  // Tests that redirecting from http to other servers works as expected
  add_update_test(HTTP,       HTTP,       SUCCESS);
  add_update_test(HTTP,       HTTPS,      SUCCESS);
  add_update_test(HTTP,       NOCERT,     DOWNLOAD_ERROR);
  add_update_test(HTTP,       SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(HTTP,       UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(HTTP,       EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from valid https to other servers works as expected
  add_update_test(HTTPS,      HTTP,       DOWNLOAD_ERROR);
  add_update_test(HTTPS,      HTTPS,      DOWNLOAD_ERROR);
  add_update_test(HTTPS,      NOCERT,     DOWNLOAD_ERROR);
  add_update_test(HTTPS,      SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(HTTPS,      UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(HTTPS,      EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from nocert https to other servers works as expected
  add_update_test(NOCERT,     HTTP,       DOWNLOAD_ERROR);
  add_update_test(NOCERT,     HTTPS,      DOWNLOAD_ERROR);
  add_update_test(NOCERT,     NOCERT,     DOWNLOAD_ERROR);
  add_update_test(NOCERT,     SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(NOCERT,     UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(NOCERT,     EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from self-signed https to other servers works as expected
  add_update_test(SELFSIGNED, HTTP,       DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, HTTPS,      DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, NOCERT,     DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from untrusted https to other servers works as expected
  add_update_test(UNTRUSTED,  HTTP,       DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  HTTPS,      DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  NOCERT,     DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from expired https to other servers works as expected
  add_update_test(EXPIRED,    HTTP,       DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    HTTPS,      DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    NOCERT,     DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    EXPIRED,    DOWNLOAD_ERROR);

  run_update_tests(run_next_test);
});

// Runs tests without requiring built-in certificates and no certificate
// exceptions.
add_test(function() {
  Services.prefs.setBoolPref(PREF_UPDATE_REQUIREBUILTINCERTS, false);

  // Tests that a simple update.rdf retrieval works as expected.
  add_update_test(HTTP,       null,       SUCCESS);
  add_update_test(HTTPS,      null,       SUCCESS);
  add_update_test(NOCERT,     null,       DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, null,       DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  null,       DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    null,       DOWNLOAD_ERROR);

  // Tests that redirecting from http to other servers works as expected
  add_update_test(HTTP,       HTTP,       SUCCESS);
  add_update_test(HTTP,       HTTPS,      SUCCESS);
  add_update_test(HTTP,       NOCERT,     DOWNLOAD_ERROR);
  add_update_test(HTTP,       SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(HTTP,       UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(HTTP,       EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from valid https to other servers works as expected
  add_update_test(HTTPS,      HTTP,       DOWNLOAD_ERROR);
  add_update_test(HTTPS,      HTTPS,      SUCCESS);
  add_update_test(HTTPS,      NOCERT,     DOWNLOAD_ERROR);
  add_update_test(HTTPS,      SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(HTTPS,      UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(HTTPS,      EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from nocert https to other servers works as expected
  add_update_test(NOCERT,     HTTP,       DOWNLOAD_ERROR);
  add_update_test(NOCERT,     HTTPS,      DOWNLOAD_ERROR);
  add_update_test(NOCERT,     NOCERT,     DOWNLOAD_ERROR);
  add_update_test(NOCERT,     SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(NOCERT,     UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(NOCERT,     EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from self-signed https to other servers works as expected
  add_update_test(SELFSIGNED, HTTP,       DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, HTTPS,      DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, NOCERT,     DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from untrusted https to other servers works as expected
  add_update_test(UNTRUSTED,  HTTP,       DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  HTTPS,      DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  NOCERT,     DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from expired https to other servers works as expected
  add_update_test(EXPIRED,    HTTP,       DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    HTTPS,      DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    NOCERT,     DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    EXPIRED,    DOWNLOAD_ERROR);

  run_update_tests(run_next_test);
});

// Runs tests with built-in certificates required and all certificate exceptions.
add_test(function() {
  Services.prefs.clearUserPref(PREF_UPDATE_REQUIREBUILTINCERTS);
  addCertOverrides();

  // Tests that a simple update.rdf retrieval works as expected.
  add_update_test(HTTP,       null,       SUCCESS);
  add_update_test(HTTPS,      null,       DOWNLOAD_ERROR);
  add_update_test(NOCERT,     null,       DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, null,       DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  null,       DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    null,       DOWNLOAD_ERROR);

  // Tests that redirecting from http to other servers works as expected
  add_update_test(HTTP,       HTTP,       SUCCESS);
  add_update_test(HTTP,       HTTPS,      SUCCESS);
  add_update_test(HTTP,       NOCERT,     SUCCESS);
  add_update_test(HTTP,       SELFSIGNED, SUCCESS);
  add_update_test(HTTP,       UNTRUSTED,  SUCCESS);
  add_update_test(HTTP,       EXPIRED,    SUCCESS);

  // Tests that redirecting from valid https to other servers works as expected
  add_update_test(HTTPS,      HTTP,       DOWNLOAD_ERROR);
  add_update_test(HTTPS,      HTTPS,      DOWNLOAD_ERROR);
  add_update_test(HTTPS,      NOCERT,     DOWNLOAD_ERROR);
  add_update_test(HTTPS,      SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(HTTPS,      UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(HTTPS,      EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from nocert https to other servers works as expected
  add_update_test(NOCERT,     HTTP,       DOWNLOAD_ERROR);
  add_update_test(NOCERT,     HTTPS,      DOWNLOAD_ERROR);
  add_update_test(NOCERT,     NOCERT,     DOWNLOAD_ERROR);
  add_update_test(NOCERT,     SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(NOCERT,     UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(NOCERT,     EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from self-signed https to other servers works as expected
  add_update_test(SELFSIGNED, HTTP,       DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, HTTPS,      DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, NOCERT,     DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from untrusted https to other servers works as expected
  add_update_test(UNTRUSTED,  HTTP,       DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  HTTPS,      DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  NOCERT,     DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  EXPIRED,    DOWNLOAD_ERROR);

  // Tests that redirecting from expired https to other servers works as expected
  add_update_test(EXPIRED,    HTTP,       DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    HTTPS,      DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    NOCERT,     DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    SELFSIGNED, DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    UNTRUSTED,  DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    EXPIRED,    DOWNLOAD_ERROR);

  run_update_tests(run_next_test);
});

// Runs tests without requiring built-in certificates and all certificate
// exceptions.
add_test(function() {
  Services.prefs.setBoolPref(PREF_UPDATE_REQUIREBUILTINCERTS, false);

  // Tests that a simple update.rdf retrieval works as expected.
  add_update_test(HTTP,       null,       SUCCESS);
  add_update_test(HTTPS,      null,       SUCCESS);
  add_update_test(NOCERT,     null,       SUCCESS);
  add_update_test(SELFSIGNED, null,       SUCCESS);
  add_update_test(UNTRUSTED,  null,       SUCCESS);
  add_update_test(EXPIRED,    null,       SUCCESS);

  // Tests that redirecting from http to other servers works as expected
  add_update_test(HTTP,       HTTP,       SUCCESS);
  add_update_test(HTTP,       HTTPS,      SUCCESS);
  add_update_test(HTTP,       NOCERT,     SUCCESS);
  add_update_test(HTTP,       SELFSIGNED, SUCCESS);
  add_update_test(HTTP,       UNTRUSTED,  SUCCESS);
  add_update_test(HTTP,       EXPIRED,    SUCCESS);

  // Tests that redirecting from valid https to other servers works as expected
  add_update_test(HTTPS,      HTTP,       DOWNLOAD_ERROR);
  add_update_test(HTTPS,      HTTPS,      SUCCESS);
  add_update_test(HTTPS,      NOCERT,     SUCCESS);
  add_update_test(HTTPS,      SELFSIGNED, SUCCESS);
  add_update_test(HTTPS,      UNTRUSTED,  SUCCESS);
  add_update_test(HTTPS,      EXPIRED,    SUCCESS);

  // Tests that redirecting from nocert https to other servers works as expected
  add_update_test(NOCERT,     HTTP,       DOWNLOAD_ERROR);
  add_update_test(NOCERT,     HTTPS,      SUCCESS);
  add_update_test(NOCERT,     NOCERT,     SUCCESS);
  add_update_test(NOCERT,     SELFSIGNED, SUCCESS);
  add_update_test(NOCERT,     UNTRUSTED,  SUCCESS);
  add_update_test(NOCERT,     EXPIRED,    SUCCESS);

  // Tests that redirecting from self-signed https to other servers works as expected
  add_update_test(SELFSIGNED, HTTP,       DOWNLOAD_ERROR);
  add_update_test(SELFSIGNED, HTTPS,      SUCCESS);
  add_update_test(SELFSIGNED, NOCERT,     SUCCESS);
  add_update_test(SELFSIGNED, SELFSIGNED, SUCCESS);
  add_update_test(SELFSIGNED, UNTRUSTED,  SUCCESS);
  add_update_test(SELFSIGNED, EXPIRED,    SUCCESS);

  // Tests that redirecting from untrusted https to other servers works as expected
  add_update_test(UNTRUSTED,  HTTP,       DOWNLOAD_ERROR);
  add_update_test(UNTRUSTED,  HTTPS,      SUCCESS);
  add_update_test(UNTRUSTED,  NOCERT,     SUCCESS);
  add_update_test(UNTRUSTED,  SELFSIGNED, SUCCESS);
  add_update_test(UNTRUSTED,  UNTRUSTED,  SUCCESS);
  add_update_test(UNTRUSTED,  EXPIRED,    SUCCESS);

  // Tests that redirecting from expired https to other servers works as expected
  add_update_test(EXPIRED,    HTTP,       DOWNLOAD_ERROR);
  add_update_test(EXPIRED,    HTTPS,      SUCCESS);
  add_update_test(EXPIRED,    NOCERT,     SUCCESS);
  add_update_test(EXPIRED,    SELFSIGNED, SUCCESS);
  add_update_test(EXPIRED,    UNTRUSTED,  SUCCESS);
  add_update_test(EXPIRED,    EXPIRED,    SUCCESS);

  run_update_tests(run_next_test);
});
