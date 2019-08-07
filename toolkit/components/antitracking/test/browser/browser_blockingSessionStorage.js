/* import-globals-from antitracking_head.js */

AntiTracking.runTestInNormalAndPrivateMode(
  "sessionStorage",
  async _ => {
    let shouldThrow =
      SpecialPowers.Services.prefs.getIntPref(
        "network.cookie.cookieBehavior"
      ) == SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT;

    let hasThrown;
    try {
      sessionStorage.foo = 42;
      hasThrown = false;
    } catch (e) {
      hasThrown = true;
      is(e.name, "SecurityError", "We want a security error message.");
    }

    is(
      hasThrown,
      shouldThrow,
      "SessionStorage show thrown only if cookieBehavior is REJECT"
    );
  },
  async _ => {
    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [],
  true,
  true,
  0
);

AntiTracking.runTest(
  "sessionStorage and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    let shouldThrow =
      SpecialPowers.Services.prefs.getIntPref(
        "network.cookie.cookieBehavior"
      ) == SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT;

    let hasThrown;
    try {
      sessionStorage.foo = 42;
      hasThrown = false;
    } catch (e) {
      hasThrown = true;
      is(e.name, "SecurityError", "We want a security error message.");
    }

    is(
      hasThrown,
      shouldThrow,
      "SessionStorage show thrown only if cookieBehavior is REJECT"
    );

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    try {
      sessionStorage.foo = 42;
      hasThrown = false;
    } catch (e) {
      hasThrown = true;
      is(e.name, "SecurityError", "We want a security error message.");
    }

    is(
      hasThrown,
      shouldThrow,
      "SessionStorage show thrown only if cookieBehavior is REJECT"
    );
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    if (allowListed) {
      await hasStorageAccessInitially();
    } else {
      await noStorageAccessInitially();
    }

    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    sessionStorage.foo = 42;
    ok(
      true,
      "SessionStorage is allowed after calling the storage access API too"
    );
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  null,
  false,
  false
);
