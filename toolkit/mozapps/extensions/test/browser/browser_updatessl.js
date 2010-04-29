/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AddonUpdateChecker.jsm");

const updaterdf = "browser/toolkit/mozapps/extensions/test/browser/browser_updatessl.rdf";
const redirect = "browser/toolkit/mozapps/extensions/test/browser/redirect.sjs?";
const SUCCESS = 0;

var gTests = [];

function test() {
  waitForExplicitFinish();

  run_next_test();
}

function end_test() {
  var cos = Cc["@mozilla.org/security/certoverride;1"].
            getService(Ci.nsICertOverrideService);
  cos.clearValidityOverride("nocert.example.com", -1);
  cos.clearValidityOverride("self-signed.example.com", -1);
  cos.clearValidityOverride("untrusted.example.com", -1);
  cos.clearValidityOverride("expired.example.com", -1);

  finish();
}

function add_update_test(url, expectedStatus, message) {
  gTests.push([url, expectedStatus, message]);
}

function run_update_tests(callback) {
  function run_next_update_test(pos) {
    if (gTests.length == 0) {
      callback();
      return;
    }

    let [url, expectedStatus, message] = gTests.shift();
    AddonUpdateChecker.checkForUpdates("addon1@tests.mozilla.org", "extension",
                                       null, url, {
      onUpdateCheckComplete: function(updates) {
        is(updates.length, 1);
        is(SUCCESS, expectedStatus, message);
        run_next_update_test();
      },

      onUpdateCheckError: function(status) {
        is(status, expectedStatus, message);
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

add_test(function() {
  // Tests that a simple update.rdf retrieval works as expected.
  add_update_test("http://example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http update url");
  add_update_test("https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https update url from a non built-in CA");
  add_update_test("https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for nocert https update url");
  add_update_test("https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for self-signed https update url");
  add_update_test("https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for untrusted https update url");
  add_update_test("https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for expired https update url");

  // Tests that redirecting from http to other servers works as expected
  add_update_test("http://example.com/" + redirect + "http://example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to http redirect");
  add_update_test("http://example.com/" + redirect + "https://example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to https redirect for a non built-in CA");
  add_update_test("http://example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a http to nocert https redirect");
  add_update_test("http://example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a http to self-signed https redirect");
  add_update_test("http://example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a http to untrusted https update url");
  add_update_test("http://example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a http to expired https update url");

  // Tests that redirecting from valid https to other servers works as expected
  add_update_test("https://example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to http redirect");
  add_update_test("https://example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to https redirect for a non built-in CA");
  add_update_test("https://example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to nocert https redirect");
  add_update_test("https://example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to self-signed https redirect");
  add_update_test("https://example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to untrusted https redirect");
  add_update_test("https://example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to expired https redirect");

  // Tests that redirecting from nocert https to other servers works as expected
  add_update_test("https://nocert.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to http redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to nocert https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to self-signed https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to untrusted https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to expired https redirect");

  // Tests that redirecting from self-signed https to other servers works as expected
  add_update_test("https://self-signed.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to http redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to nocert https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to self-signed https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to untrusted https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to expired https redirect");

  // Tests that redirecting from untrusted https to other servers works as expected
  add_update_test("https://untrusted.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to http redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to nocert https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to self-signed https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to untrusted https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to expired https redirect");

  // Tests that redirecting from expired https to other servers works as expected
  add_update_test("https://expired.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to http redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to nocert https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to self-signed https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to untrusted https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to expired https redirect");

  run_update_tests(run_next_test);
});

add_test(function() {
  addCertOverrides();

  // Tests that a simple update.rdf retrieval works as expected.
  add_update_test("http://example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http update url");
  add_update_test("https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https update url from a non built-in CA");
  add_update_test("https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for nocert https update url");
  add_update_test("https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for self-signed https update url");
  add_update_test("https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for untrusted https update url");
  add_update_test("https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for expired https update url");

  // Tests that redirecting from http to other servers works as expected
  add_update_test("http://example.com/" + redirect + "http://example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to http redirect");
  add_update_test("http://example.com/" + redirect + "https://example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to https redirect for a non built-in CA");
  add_update_test("http://example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to nocert https redirect");
  add_update_test("http://example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to self-signed https redirect");
  add_update_test("http://example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to untrusted https update url");
  add_update_test("http://example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  SUCCESS,
                  "Should have seen no failure for a http to expired https update url");

  // Tests that redirecting from valid https to other servers works as expected
  add_update_test("https://example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to http redirect");
  add_update_test("https://example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to https redirect for a non built-in CA");
  add_update_test("https://example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to nocert https redirect");
  add_update_test("https://example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to self-signed https redirect");
  add_update_test("https://example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to untrusted https redirect");
  add_update_test("https://example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a https to expired https redirect");

  // Tests that redirecting from nocert https to other servers works as expected
  add_update_test("https://nocert.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to http redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to nocert https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to self-signed https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to untrusted https redirect");
  add_update_test("https://nocert.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a nocert https to expired https redirect");

  // Tests that redirecting from self-signed https to other servers works as expected
  add_update_test("https://self-signed.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to http redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to nocert https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to self-signed https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to untrusted https redirect");
  add_update_test("https://self-signed.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a self-signed https to expired https redirect");

  // Tests that redirecting from untrusted https to other servers works as expected
  add_update_test("https://untrusted.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to http redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to nocert https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to self-signed https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to untrusted https redirect");
  add_update_test("https://untrusted.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a untrusted https to expired https redirect");

  // Tests that redirecting from expired https to other servers works as expected
  add_update_test("https://expired.example.com/" + redirect + "http://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to http redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://nocert.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to nocert https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://self-signed.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to self-signed https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://untrusted.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to untrusted https redirect");
  add_update_test("https://expired.example.com/" + redirect + "https://expired.example.com/" + updaterdf,
                  AddonUpdateChecker.ERROR_DOWNLOAD_ERROR,
                  "Should have seen a failure for a expired https to expired https redirect");

  run_update_tests(run_next_test);
});
