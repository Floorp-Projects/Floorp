/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/hawkrequest.js");

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
    let acceptLanguage = Cc["@mozilla.org/supports-string;1"]
                           .createInstance(Ci.nsISupportsString);
    acceptLanguage.data = lang;
    Services.prefs.setComplexValue(
      "intl.accept_languages", Ci.nsISupportsString, acceptLanguage);
  }

  let hawk = new HAWKAuthenticatedRESTRequest("https://example.com");

  Services.prefs.addObserver("intl.accept_languages", checkLanguagePref, false);
  setLanguagePref(languages[testCount]);

  function checkLanguagePref() {
    var _done = false;
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
      return;
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
  let acceptLanguage = Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);
  acceptLanguage.data = 'zu-NP'; // omit trailing ';', which our HTTP libs snip
  Services.prefs.setComplexValue('intl.accept_languages', Ci.nsISupportsString, acceptLanguage);

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
    do_check_neq(acceptLanguage.data, pref.data);

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
    let acceptLanguage = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    acceptLanguage.data = lang;
    Services.prefs.setComplexValue("intl.accept_languages", Ci.nsISupportsString, acceptLanguage);
  }

  let server = httpd_setup({
    "/foo": function(request, response) {
      do_check_eq(languages[1], request.getHeader("Accept-Language"));

      response.setStatusLine(request.httpVersion, 200, "OK");
    },
  });

  let url = server.baseURI + "/foo";
  let postData = {};
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

