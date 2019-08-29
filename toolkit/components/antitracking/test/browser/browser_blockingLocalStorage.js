/* import-globals-from antitracking_head.js */

AntiTracking.runTestInNormalAndPrivateMode(
  "localStorage",
  async _ => {
    is(window.localStorage, null, "LocalStorage is null");
    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "TypeError", "We want a type error message.");
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
  }
);

AntiTracking.runTestInNormalAndPrivateMode(
  "localStorage and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    is(window.localStorage, null, "LocalStorage is null");
    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "TypeError", "We want a type error message.");
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    if (
      [
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
      ].includes(
        SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior")
      )
    ) {
      is(window.localStorage, null, "LocalStorage is null");
      try {
        localStorage.foo = 42;
        ok(false, "LocalStorage cannot be used!");
      } catch (e) {
        ok(true, "LocalStorage cannot be used!");
        is(e.name, "TypeError", "We want a type error message.");
      }
    } else {
      localStorage.foo = 42;
      ok(true, "LocalStorage is allowed");
    }
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    if (allowListed) {
      await hasStorageAccessInitially();
    } else {
      await noStorageAccessInitially();
    }

    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
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
  null,
  false,
  false
);
