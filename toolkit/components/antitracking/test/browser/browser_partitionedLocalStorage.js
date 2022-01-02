/* import-globals-from antitracking_head.js */
/* import-globals-from partitionedstorage_head.js */

AntiTracking.runTestInNormalAndPrivateMode(
  "localStorage and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    let effectiveCookieBehavior = SpecialPowers.isContentWindowPrivate(window)
      ? SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior.pbmode"
        )
      : SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior"
        );

    let shouldThrow = [
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    ].includes(effectiveCookieBehavior);

    let hasThrown;
    try {
      localStorage.foo = 42;
      ok(true, "LocalStorage is allowed");
      is(localStorage.foo, "42", "The value matches");
      hasThrown = false;
    } catch (e) {
      is(e.name, "SecurityError", "We want a security error message.");
      hasThrown = true;
    }

    is(hasThrown, shouldThrow, "LocalStorage has been exposed correctly");

    let prevLocalStorage;
    if (!shouldThrow) {
      prevLocalStorage = localStorage;
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    if (shouldThrow) {
      try {
        is(localStorage.foo, undefined, "Undefined value after.");
        ok(false, "localStorage should not be available");
      } catch (e) {
        ok(true, "localStorage should not be available");
      }
    } else {
      ok(localStorage != prevLocalStorage, "We have a new localStorage");
      is(localStorage.foo, undefined, "Undefined value after.");

      localStorage.foo = 42;
      ok(true, "LocalStorage is still allowed");
      is(localStorage.foo, "42", "The value matches");
    }
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
    is(localStorage.foo, "42", "The value matches");

    var prevLocalStorage = localStorage;

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    ok(localStorage == prevLocalStorage, "We have a new localStorage");
    is(localStorage.foo, "42", "The value matches");
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
      "privacy.restrict3rdpartystorage.partitionedHosts",
      "tracking.example.org,tracking.example.com",
    ],
  ],
  false,
  false
);

PartitionedStorageHelper.runPartitioningTestInNormalAndPrivateMode(
  "Partitioned tabs - localStorage",
  "localstorage",

  // getDataCallback
  async win => {
    return "foo" in win.localStorage ? win.localStorage.foo : "";
  },

  // addDataCallback
  async (win, value) => {
    win.localStorage.foo = value;
    return true;
  },

  // cleanup
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);
