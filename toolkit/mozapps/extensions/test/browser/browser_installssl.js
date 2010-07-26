/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const xpi = RELATIVE_DIR + "addons/browser_installssl.xpi";
const redirect = RELATIVE_DIR + "redirect.sjs?";
const SUCCESS = 0;

var gTests = [];
var gStart = 0;
var gLast = 0;
var gPendingInstall = null;

function test() {
  gStart = Date.now();
  requestLongerTimeout(2);
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    if (gPendingInstall) {
      gTests = [];
      ok(false, "Timed out in the middle of downloading " + gPendingInstall.sourceURI.spec);
      try {
        gPendingInstall.cancel();
      }
      catch (e) {
      }
    }
  });

  run_next_test();
}

function end_test() {
  var cos = Cc["@mozilla.org/security/certoverride;1"].
            getService(Ci.nsICertOverrideService);
  cos.clearValidityOverride("nocert.example.com", -1);
  cos.clearValidityOverride("self-signed.example.com", -1);
  cos.clearValidityOverride("untrusted.example.com", -1);
  cos.clearValidityOverride("expired.example.com", -1);

  info("All tests completed in " + (Date.now() - gStart) + "ms");
  finish();
}

function add_install_test(url, expectedStatus, message) {
  gTests.push([url, expectedStatus, message]);
}

function run_install_tests(callback) {
  function run_next_install_test() {
    if (gTests.length == 0) {
      callback();
      return;
    }
    gLast = Date.now();

    let [url, expectedStatus, message] = gTests.shift();
    AddonManager.getInstallForURL(url, function(install) {
      gPendingInstall = install;
      install.addListener({
        onDownloadEnded: function(install) {
          is(SUCCESS, expectedStatus, message);
          info("Install test ran in " + (Date.now() - gLast) + "ms");
          // Don't proceed with the install
          install.cancel();
          gPendingInstall = null;
          run_next_install_test();
          return false;
        },

        onDownloadFailed: function(install) {
          is(install.error, expectedStatus, message);
          info("Install test ran in " + (Date.now() - gLast) + "ms");
          gPendingInstall = null;
          run_next_install_test();
        }
      });
      install.install();
    }, "application/x-xpinstall");
  }

  run_next_install_test();
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
  add_install_test("http://example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http install url");
  add_install_test("https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https install url from a non built-in CA");
  add_install_test("https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for nocert https install url");
  add_install_test("https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for self-signed https install url");
  add_install_test("https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for untrusted https install url");
  add_install_test("https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for expired https install url");

  // Tests that redirecting from http to other servers works as expected
  add_install_test("http://example.com/" + redirect + "http://example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to http redirect");
  add_install_test("http://example.com/" + redirect + "https://example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to https redirect for a non built-in CA");
  add_install_test("http://example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a http to nocert https redirect");
  add_install_test("http://example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a http to self-signed https redirect");
  add_install_test("http://example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a http to untrusted https install url");
  add_install_test("http://example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a http to expired https install url");

  // Tests that redirecting from valid https to other servers works as expected
  add_install_test("https://example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to http redirect");
  add_install_test("https://example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to https redirect for a non built-in CA");
  add_install_test("https://example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to nocert https redirect");
  add_install_test("https://example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to self-signed https redirect");
  add_install_test("https://example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to untrusted https redirect");
  add_install_test("https://example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to expired https redirect");

  // Tests that redirecting from nocert https to other servers works as expected
  add_install_test("https://nocert.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to http redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to nocert https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to self-signed https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to untrusted https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to expired https redirect");

  // Tests that redirecting from self-signed https to other servers works as expected
  add_install_test("https://self-signed.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to http redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to nocert https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to self-signed https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to untrusted https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to expired https redirect");

  // Tests that redirecting from untrusted https to other servers works as expected
  add_install_test("https://untrusted.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to http redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to nocert https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to self-signed https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to untrusted https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to expired https redirect");

  // Tests that redirecting from expired https to other servers works as expected
  add_install_test("https://expired.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to http redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to nocert https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to self-signed https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to untrusted https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to expired https redirect");

  run_install_tests(run_next_test);
});

add_test(function() {
  addCertOverrides();

  // Tests that a simple update.rdf retrieval works as expected.
  add_install_test("http://example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http install url");
  add_install_test("https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https install url from a non built-in CA");
  add_install_test("https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for nocert https install url");
  add_install_test("https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for self-signed https install url");
  add_install_test("https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for untrusted https install url");
  add_install_test("https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for expired https install url");

  // Tests that redirecting from http to other servers works as expected
  add_install_test("http://example.com/" + redirect + "http://example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to http redirect");
  add_install_test("http://example.com/" + redirect + "https://example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to https redirect for a non built-in CA");
  add_install_test("http://example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to nocert https redirect");
  add_install_test("http://example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to self-signed https redirect");
  add_install_test("http://example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to untrusted https install url");
  add_install_test("http://example.com/" + redirect + "https://expired.example.com/" + xpi,
                   SUCCESS,
                   "Should have seen no failure for a http to expired https install url");

  // Tests that redirecting from valid https to other servers works as expected
  add_install_test("https://example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to http redirect");
  add_install_test("https://example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to https redirect for a non built-in CA");
  add_install_test("https://example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to nocert https redirect");
  add_install_test("https://example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to self-signed https redirect");
  add_install_test("https://example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to untrusted https redirect");
  add_install_test("https://example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a https to expired https redirect");

  // Tests that redirecting from nocert https to other servers works as expected
  add_install_test("https://nocert.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to http redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to nocert https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to self-signed https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to untrusted https redirect");
  add_install_test("https://nocert.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a nocert https to expired https redirect");

  // Tests that redirecting from self-signed https to other servers works as expected
  add_install_test("https://self-signed.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to http redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to nocert https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to self-signed https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to untrusted https redirect");
  add_install_test("https://self-signed.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a self-signed https to expired https redirect");

  // Tests that redirecting from untrusted https to other servers works as expected
  add_install_test("https://untrusted.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to http redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to nocert https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to self-signed https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to untrusted https redirect");
  add_install_test("https://untrusted.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a untrusted https to expired https redirect");

  // Tests that redirecting from expired https to other servers works as expected
  add_install_test("https://expired.example.com/" + redirect + "http://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to http redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://nocert.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to nocert https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://self-signed.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to self-signed https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://untrusted.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to untrusted https redirect");
  add_install_test("https://expired.example.com/" + redirect + "https://expired.example.com/" + xpi,
                   AddonManager.ERROR_NETWORK_FAILURE,
                   "Should have seen a failure for a expired https to expired https redirect");

  run_install_tests(run_next_test);
});
