/* import-globals-from antitracking_head.js */

AntiTracking.runTest(
  "localStorage with a tracker that is entitylisted via a pref",
  async _ => {
    let shouldThrow = [
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    ].includes(
      SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior")
    );

    let hasThrown;
    try {
      localStorage.foo = 42;
      hasThrown = false;
    } catch (e) {
      hasThrown = true;
    }

    is(hasThrown, shouldThrow, "LocalStorage is allowed");
  },
  async _ => {
    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [["urlclassifier.trackingAnnotationSkipURLs", "TRACKING.EXAMPLE.ORG"]],
  false, // run the window.open() test
  false, // run the user interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  false
); // run in a normal window

AntiTracking.runTest(
  "localStorage with a tracker that is entitylisted via a fancy pref",
  async _ => {
    let shouldThrow = [
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    ].includes(
      SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior")
    );

    let hasThrown;
    try {
      localStorage.foo = 42;
      hasThrown = false;
    } catch (e) {
      hasThrown = true;
    }

    is(hasThrown, shouldThrow, "LocalStorage is allowed");
  },
  async _ => {
    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [
    [
      "urlclassifier.trackingAnnotationSkipURLs",
      "foobar.example,*.example.org,baz.example",
    ],
  ],
  false, // run the window.open() test
  false, // run the user interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  false
); // run in a normal window

AntiTracking.runTest(
  "localStorage with a tracker that is entitylisted via a misconfigured pref",
  async _ => {
    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  async _ => {
    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [["urlclassifier.trackingAnnotationSkipURLs", "*.tracking.example.org"]],
  false, // run the window.open() test
  false, // run the user interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  false
); // run in a normal window
