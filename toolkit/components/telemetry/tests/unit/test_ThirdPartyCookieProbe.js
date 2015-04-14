/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/ThirdPartyCookieProbe.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);

let TOPIC_ACCEPTED = "third-party-cookie-accepted";
let TOPIC_REJECTED = "third-party-cookie-rejected";

let FLUSH_MILLISECONDS = 1000 * 60 * 60 * 24 / 2; /*Half a day, for testing purposes*/

const NUMBER_OF_REJECTS = 30;
const NUMBER_OF_ACCEPTS = 17;
const NUMBER_OF_REPEATS = 5;

let gCookieService;
let gThirdPartyCookieProbe;

let gHistograms = {
  clear: function() {
    this.sitesAccepted.clear();
    this.requestsAccepted.clear();
    this.sitesRejected.clear();
    this.requestsRejected.clear();
  }
};

function run_test() {
  do_print("Initializing environment");
  do_get_profile();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  gCookieService = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  do_print("Initializing ThirdPartyCookieProbe.jsm");
  gThirdPartyCookieProbe = new ThirdPartyCookieProbe();
  gThirdPartyCookieProbe.init();

  do_print("Acquiring histograms");
  gHistograms.sitesAccepted =
    Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_SITES_ACCEPTED");
  gHistograms.sitesRejected =
    Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_SITES_BLOCKED"),
  gHistograms.requestsAccepted =
    Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_ATTEMPTS_ACCEPTED");
  gHistograms.requestsRejected =
    Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_ATTEMPTS_BLOCKED"),


  run_next_test();
}

/**
 * Utility function: try to set a cookie with the given document uri and referrer uri.
 *
 * @param obj An object with the following fields
 * - {string} request The uri of the request setting the cookie.
 * - {string} referrer The uri of the referrer for this request.
 */
function tryToSetCookie(obj) {
  let requestURI = Services.io.newURI(obj.request, null, null);
  let referrerURI = Services.io.newURI(obj.referrer, null, null);
  let requestChannel = Services.io.newChannelFromURI2(requestURI,
                                                      null,      // aLoadingNode
                                                      Services.scriptSecurityManager.getSystemPrincipal(),
                                                      null,      // aTriggeringPrincipal
                                                      Ci.nsILoadInfo.SEC_NORMAL,
                                                      Ci.nsIContentPolicy.TYPE_OTHER);
  gCookieService.setCookieString(referrerURI, null, "Is there a cookie in my jar?", requestChannel);
}

function wait(ms) {
  let deferred = Promise.defer();
  do_timeout(ms, () => deferred.resolve());
  return deferred.promise;
}

function oneTest(tld, flushUptime, check) {
  gHistograms.clear();
  do_print("Testing with tld " + tld);

  do_print("Adding rejected entries");
  Services.prefs.setIntPref("network.cookie.cookieBehavior",
                          1 /*reject third-party cookies*/);

  for (let i = 0; i < NUMBER_OF_REJECTS; ++i) {
    for (let j = 0; j < NUMBER_OF_REPEATS; ++j) {
      for (let prefix of ["http://", "https://"]) {
        // Histogram sitesRejected should only count
        // NUMBER_OF_REJECTS entries.
        // Histogram requestsRejected should count
        // NUMBER_OF_REJECTS * NUMBER_OF_REPEATS * 2
        tryToSetCookie({
          request: prefix + "echelon" + tld,
          referrer: prefix + "domain" + i + tld
        });
      }
    }
  }

  do_print("Adding accepted entries");
  Services.prefs.setIntPref("network.cookie.cookieBehavior",
                            0 /*accept third-party cookies*/);

  for (let i = 0; i < NUMBER_OF_ACCEPTS; ++i) {
    for (let j = 0; j < NUMBER_OF_REPEATS; ++j) {
      for (let prefix of ["http://", "https://"]) {
        // Histogram sitesAccepted should only count
        // NUMBER_OF_ACCEPTS entries.
        // Histogram requestsAccepted should count
        // NUMBER_OF_ACCEPTS * NUMBER_OF_REPEATS * 2
        tryToSetCookie({
            request: prefix + "prism" + tld,
            referrer: prefix + "domain" + i + tld
          });
      }
    }
  }

  do_print("Checking that the histograms have not changed before ping()");
  do_check_eq(gHistograms.sitesAccepted.snapshot().sum, 0);
  do_check_eq(gHistograms.sitesRejected.snapshot().sum, 0);
  do_check_eq(gHistograms.requestsAccepted.snapshot().sum, 0);
  do_check_eq(gHistograms.requestsRejected.snapshot().sum, 0);

  do_print("Checking that the resulting histograms are correct");
  if (flushUptime != null) {
    let now = Date.now();
    let before = now - flushUptime;
    gThirdPartyCookieProbe._latestFlush = before;
    gThirdPartyCookieProbe.flush(now);
  } else {
    gThirdPartyCookieProbe.flush();
  }
  check();
}

add_task(function() {
  // To ensure that we work correctly with eTLD, test with several suffixes
  for (let tld of [".com", ".com.ar", ".co.uk", ".gouv.fr"]) {
    oneTest(tld, FLUSH_MILLISECONDS, function() {
      do_check_eq(gHistograms.sitesAccepted.snapshot().sum, NUMBER_OF_ACCEPTS * 2);
      do_check_eq(gHistograms.sitesRejected.snapshot().sum, NUMBER_OF_REJECTS * 2);
      do_check_eq(gHistograms.requestsAccepted.snapshot().sum, NUMBER_OF_ACCEPTS * NUMBER_OF_REPEATS * 2 * 2);
      do_check_eq(gHistograms.requestsRejected.snapshot().sum, NUMBER_OF_REJECTS * NUMBER_OF_REPEATS * 2 * 2);
    });
  }

  // Check that things still work with default uptime management
  for (let tld of [".com", ".com.ar", ".co.uk", ".gouv.fr"]) {
    yield wait(1000); // Ensure that uptime is at least one second
    oneTest(tld, null, function() {
      do_check_true(gHistograms.sitesAccepted.snapshot().sum > 0);
      do_check_true(gHistograms.sitesRejected.snapshot().sum > 0);
      do_check_true(gHistograms.requestsAccepted.snapshot().sum > 0);
      do_check_true(gHistograms.requestsRejected.snapshot().sum > 0);
    });
  }
});

add_task(function() {
  gThirdPartyCookieProbe.dispose();
});

