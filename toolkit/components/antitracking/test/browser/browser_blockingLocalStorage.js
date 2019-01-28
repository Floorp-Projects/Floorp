AntiTracking.runTest("localStorage",
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
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

AntiTracking.runTest("localStorage and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    if (SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior") == SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT) {
      try {
        localStorage.foo = 42;
        ok(false, "LocalStorage cannot be used!");
      } catch (e) {
        ok(true, "LocalStorage cannot be used!");
        is(e.name, "SecurityError", "We want a security error message.");
      }
    } else {
      localStorage.foo = 42;
      ok(true, "LocalStorage is allowed");
    }
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

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
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);
