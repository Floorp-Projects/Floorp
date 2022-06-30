/* import-globals-from antitracking_head.js */

requestLongerTimeout(4);

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
  },
  [["dom.indexedDB.hide_in_pbmode.enabled", false]]
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
      indexedDB.open("test", "1");
      hasThrown = false;
    } catch (e) {
      hasThrown = true;
      is(e.name, "SecurityError", "We want a security error message.");
    }

    is(
      hasThrown,
      shouldThrow,
      "IDB should be allowed if not in cookieBehavior pref value BEHAVIOR_REJECT/BEHAVIOR_REJECT_FOREIGN"
    );
  },
  // non-blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

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
  [["dom.indexedDB.hide_in_pbmode.enabled", false]],
  false,
  false
);
