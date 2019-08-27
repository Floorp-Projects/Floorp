/* import-globals-from antitracking_head.js */

AntiTracking.runTestInNormalAndPrivateMode(
  "IndexedDB",
  // blocking callback
  async _ => {
    try {
      indexedDB.open("test", "1");
      ok(false, "IDB should be blocked");
    } catch (e) {
      ok(true, "IDB should be blocked");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  // non-blocking callback
  async _ => {
    indexedDB.open("test", "1");
    ok(true, "IDB should be allowed");
  },
  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

AntiTracking.runTestInNormalAndPrivateMode(
  "IndexedDB and Storage Access API",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    try {
      indexedDB.open("test", "1");
      ok(false, "IDB should be blocked");
    } catch (e) {
      ok(true, "IDB should be blocked");
      is(e.name, "SecurityError", "We want a security error message.");
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    let shouldThrow =
      SpecialPowers.Services.prefs.getIntPref(
        "network.cookie.cookieBehavior"
      ) == SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT;

    let hasThrown;
    try {
      indexedDB.open("test", "1");
      hasThrown = false;
    } catch (e) {
      hasThrown = true;
      is(e.name, "SecurityError", "We want a security error message.");
    }

    is(
      hasThrown,
      shouldThrow,
      "IDB should be allowed if not in BEHAVIOR_REJECT"
    );
  },
  // non-blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    if (allowListed) {
      await hasStorageAccessInitially();
    } else {
      await noStorageAccessInitially();
    }

    indexedDB.open("test", "1");
    ok(true, "IDB should be allowed");

    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    indexedDB.open("test", "1");
    ok(true, "IDB should be allowed");
  },
  // Cleanup callback
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
