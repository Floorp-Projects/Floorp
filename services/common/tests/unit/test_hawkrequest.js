/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  HAWKAuthenticatedRESTRequest,
  deriveHawkCredentials,
} = ChromeUtils.import("resource://services-common/hawkrequest.js");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Async } = ChromeUtils.import("resource://services-common/async.js");

// https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#wiki-use-session-certificatesign-etc
var SESSION_KEYS = {
  sessionToken: h(
    // eslint-disable-next-line no-useless-concat
    "a0a1a2a3a4a5a6a7 a8a9aaabacadaeaf" + "b0b1b2b3b4b5b6b7 b8b9babbbcbdbebf"
  ),

  tokenID: h(
    // eslint-disable-next-line no-useless-concat
    "c0a29dcf46174973 da1378696e4c82ae" + "10f723cf4f4d9f75 e39f4ae3851595ab"
  ),

  reqHMACkey: h(
    // eslint-disable-next-line no-useless-concat
    "9d8f22998ee7f579 8b887042466b72d5" + "3e56ab0c094388bf 65831f702d2febc0"
  ),
};

function do_register_cleanup() {
  Services.prefs.clearUserPref("intl.accept_languages");
  Services.prefs.clearUserPref("services.common.log.logger.rest.request");

  // remove the pref change listener
  let hawk = new HAWKAuthenticatedRESTRequest("https://example.com");
  hawk._intl.uninit();
}

function run_test() {
  registerCleanupFunction(do_register_cleanup);

  Services.prefs.setStringPref(
    "services.common.log.logger.rest.request",
    "Trace"
  );
  initTestLogging("Trace");

  run_next_test();
}

add_test(function test_intl_accept_language() {
  let testCount = 0;
  let languages = [
    "zu-NP;vo", // Nepalese dialect of Zulu, defaulting to Volapük
    "fa-CG;ik", // Congolese dialect of Farsei, defaulting to Inupiaq
  ];

  function setLanguagePref(lang) {
    Services.prefs.setStringPref("intl.accept_languages", lang);
  }

  let hawk = new HAWKAuthenticatedRESTRequest("https://example.com");

  Services.prefs.addObserver("intl.accept_languages", checkLanguagePref);
  setLanguagePref(languages[testCount]);

  function checkLanguagePref() {
    CommonUtils.nextTick(function() {
      // Ensure we're only called for the number of entries in languages[].
      Assert.ok(testCount < languages.length);

      Assert.equal(hawk._intl.accept_languages, languages[testCount]);

      testCount++;
      if (testCount < languages.length) {
        // Set next language in prefs; Pref service will call checkNextLanguage.
        setLanguagePref(languages[testCount]);
        return;
      }

      // We've checked all the entries in languages[]. Cleanup and move on.
      info(
        "Checked " +
          testCount +
          " languages. Removing checkLanguagePref as pref observer."
      );
      Services.prefs.removeObserver("intl.accept_languages", checkLanguagePref);
      run_next_test();
    });
  }
});

add_task(async function test_hawk_authenticated_request() {
  let postData = { your: "data" };

  // An arbitrary date - Feb 2, 1971.  It ends in a bunch of zeroes to make our
  // computation with the hawk timestamp easier, since hawk throws away the
  // millisecond values.
  let then = 34329600000;

  let clockSkew = 120000;
  let timeOffset = -1 * clockSkew;
  let localTime = then + clockSkew;

  // Set the accept-languages pref to the Nepalese dialect of Zulu.
  let acceptLanguage = "zu-NP"; // omit trailing ';', which our HTTP libs snip
  Services.prefs.setStringPref("intl.accept_languages", acceptLanguage);

  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };

  let server = httpd_setup({
    "/elysium": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));

      // check that the header timestamp is our arbitrary system date, not
      // today's date.  Note that hawk header timestamps are in seconds, not
      // milliseconds.
      let authorization = request.getHeader("Authorization");
      let tsMS = parseInt(/ts="(\d+)"/.exec(authorization)[1], 10) * 1000;
      Assert.equal(tsMS, then);

      // This testing can be a little wonky. In an environment where
      //   pref("intl.accept_languages") === 'en-US, en'
      // the header is sent as:
      //   'en-US,en;q=0.5'
      // hence our fake value for acceptLanguage.
      let lang = request.getHeader("Accept-Language");
      Assert.equal(lang, acceptLanguage);

      let message = "yay";
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let url = server.baseURI + "/elysium";
  let extra = {
    now: localTime,
    localtimeOffsetMsec: timeOffset,
  };

  let request = new HAWKAuthenticatedRESTRequest(url, credentials, extra);

  // Allow hawk._intl to respond to the language pref change
  await Async.promiseYield();

  await request.post(postData);
  Assert.equal(200, request.response.status);
  Assert.equal(request.response.body, "yay");

  Services.prefs.clearUserPref("intl.accept_languages");
  let pref = Services.prefs.getComplexValue(
    "intl.accept_languages",
    Ci.nsIPrefLocalizedString
  );
  Assert.notEqual(acceptLanguage, pref.data);

  await promiseStopServer(server);
});

add_task(async function test_hawk_language_pref_changed() {
  let languages = [
    "zu-NP", // Nepalese dialect of Zulu
    "fa-CG", // Congolese dialect of Farsi
  ];

  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };

  function setLanguage(lang) {
    Services.prefs.setStringPref("intl.accept_languages", lang);
  }

  let server = httpd_setup({
    "/foo": function(request, response) {
      Assert.equal(languages[1], request.getHeader("Accept-Language"));

      response.setStatusLine(request.httpVersion, 200, "OK");
    },
  });

  let url = server.baseURI + "/foo";
  let request;

  setLanguage(languages[0]);

  // A new request should create the stateful object for tracking the current
  // language.
  request = new HAWKAuthenticatedRESTRequest(url, credentials);

  // Wait for change to propagate
  await Async.promiseYield();
  Assert.equal(languages[0], request._intl.accept_languages);

  // Change the language pref ...
  setLanguage(languages[1]);

  await Async.promiseYield();

  request = new HAWKAuthenticatedRESTRequest(url, credentials);
  let response = await request.post({});

  Assert.equal(200, response.status);
  Services.prefs.clearUserPref("intl.accept_languages");

  await promiseStopServer(server);
});

add_task(async function test_deriveHawkCredentials() {
  let credentials = await deriveHawkCredentials(
    SESSION_KEYS.sessionToken,
    "sessionToken"
  );
  Assert.equal(credentials.id, SESSION_KEYS.tokenID);
  Assert.equal(
    CommonUtils.bytesAsHex(credentials.key),
    SESSION_KEYS.reqHMACkey
  );
});

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}
