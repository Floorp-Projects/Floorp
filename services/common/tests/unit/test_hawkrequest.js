/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/hawkrequest.js");

// https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#wiki-use-session-certificatesign-etc
var SESSION_KEYS = {
  sessionToken: h("a0a1a2a3a4a5a6a7 a8a9aaabacadaeaf" +
                  "b0b1b2b3b4b5b6b7 b8b9babbbcbdbebf"),

  tokenID:      h("c0a29dcf46174973 da1378696e4c82ae" +
                  "10f723cf4f4d9f75 e39f4ae3851595ab"),

  reqHMACkey:   h("9d8f22998ee7f579 8b887042466b72d5" +
                  "3e56ab0c094388bf 65831f702d2febc0"),
};

function do_register_cleanup() {
  Services.prefs.resetUserPrefs();

  // remove the pref change listener
  let hawk = new HAWKAuthenticatedRESTRequest("https://example.com");
  hawk._intl.uninit();
}

function run_test() {
  Log.repository.getLogger("Services.Common.RESTRequest").level =
    Log.Level.Trace;
  initTestLogging("Trace");

  run_next_test();
}


add_test(function test_intl_accept_language() {
  let testCount = 0;
  let languages = [
    "zu-NP;vo",     // Nepalese dialect of Zulu, defaulting to Volapük
    "fa-CG;ik",     // Congolese dialect of Farsei, defaulting to Inupiaq
  ];

  function setLanguagePref(lang) {
    Services.prefs.setStringPref("intl.accept_languages", lang);
  }

  let hawk = new HAWKAuthenticatedRESTRequest("https://example.com");

  Services.prefs.addObserver("intl.accept_languages", checkLanguagePref, false);
  setLanguagePref(languages[testCount]);

  function checkLanguagePref() {
    CommonUtils.nextTick(function() {
      // Ensure we're only called for the number of entries in languages[].
      do_check_true(testCount < languages.length);

      do_check_eq(hawk._intl.accept_languages, languages[testCount]);

      testCount++;
      if (testCount < languages.length) {
        // Set next language in prefs; Pref service will call checkNextLanguage.
        setLanguagePref(languages[testCount]);
        return;
      }

      // We've checked all the entries in languages[]. Cleanup and move on.
      do_print("Checked " + testCount + " languages. Removing checkLanguagePref as pref observer.");
      Services.prefs.removeObserver("intl.accept_languages", checkLanguagePref);
      run_next_test();
    });
  }
});

add_test(function test_hawk_authenticated_request() {
  let onProgressCalled = false;
  let postData = {your: "data"};

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
    algorithm: "sha256"
  };

  let server = httpd_setup({
    "/elysium": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));

      // check that the header timestamp is our arbitrary system date, not
      // today's date.  Note that hawk header timestamps are in seconds, not
      // milliseconds.
      let authorization = request.getHeader("Authorization");
      let tsMS = parseInt(/ts="(\d+)"/.exec(authorization)[1], 10) * 1000;
      do_check_eq(tsMS, then);

      // This testing can be a little wonky. In an environment where
      //   pref("intl.accept_languages") === 'en-US, en'
      // the header is sent as:
      //   'en-US,en;q=0.5'
      // hence our fake value for acceptLanguage.
      let lang = request.getHeader("Accept-Language");
      do_check_eq(lang, acceptLanguage);

      let message = "yay";
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  function onProgress() {
    onProgressCalled = true;
  }

  function onComplete(error) {
    do_check_eq(200, this.response.status);
    do_check_eq(this.response.body, "yay");
    do_check_true(onProgressCalled);

    Services.prefs.resetUserPrefs();
    let pref = Services.prefs.getComplexValue(
      "intl.accept_languages", Ci.nsIPrefLocalizedString);
    do_check_neq(acceptLanguage, pref.data);

    server.stop(run_next_test);
  }

  let url = server.baseURI + "/elysium";
  let extra = {
    now: localTime,
    localtimeOffsetMsec: timeOffset
  };

  let request = new HAWKAuthenticatedRESTRequest(url, credentials, extra);

  // Allow hawk._intl to respond to the language pref change
  CommonUtils.nextTick(function() {
    request.post(postData, onComplete, onProgress);
  });
});

add_test(function test_hawk_language_pref_changed() {
  let languages = [
    "zu-NP",        // Nepalese dialect of Zulu
    "fa-CG",        // Congolese dialect of Farsi
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
      do_check_eq(languages[1], request.getHeader("Accept-Language"));

      response.setStatusLine(request.httpVersion, 200, "OK");
    },
  });

  let url = server.baseURI + "/foo";
  let request;

  setLanguage(languages[0]);

  // A new request should create the stateful object for tracking the current
  // language.
  request = new HAWKAuthenticatedRESTRequest(url, credentials);
  CommonUtils.nextTick(testFirstLanguage);

  function testFirstLanguage() {
    do_check_eq(languages[0], request._intl.accept_languages);

    // Change the language pref ...
    setLanguage(languages[1]);
    CommonUtils.nextTick(testRequest);
  }

  function testRequest() {
    // Change of language pref should be picked up, which we can see on the
    // server by inspecting the request headers.
    request = new HAWKAuthenticatedRESTRequest(url, credentials);
    request.post({}, function(error) {
      do_check_null(error);
      do_check_eq(200, this.response.status);

      Services.prefs.resetUserPrefs();

      server.stop(run_next_test);
    });
  }
});

add_task(function test_deriveHawkCredentials() {
  let credentials = deriveHawkCredentials(
    SESSION_KEYS.sessionToken, "sessionToken");

  do_check_eq(credentials.algorithm, "sha256");
  do_check_eq(credentials.id, SESSION_KEYS.tokenID);
  do_check_eq(CommonUtils.bytesAsHex(credentials.key), SESSION_KEYS.reqHMACkey);
});

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}
