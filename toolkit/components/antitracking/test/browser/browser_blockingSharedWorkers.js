/* import-globals-from antitracking_head.js */

requestLongerTimeout(4);

AntiTracking.runTestInNormalAndPrivateMode(
  "SharedWorkers",
  async _ => {
    try {
      new SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  async _ => {
    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");
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
  "SharedWorkers and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    try {
      new SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
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

    if (
      [
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
      ].includes(effectiveCookieBehavior)
    ) {
      try {
        new SharedWorker("a.js", "foo");
        ok(false, "SharedWorker cannot be used!");
      } catch (e) {
        ok(true, "SharedWorker cannot be used!");
        is(e.name, "SecurityError", "We want a security error message.");
      }
    } else {
      new SharedWorker("a.js", "foo");
      ok(true, "SharedWorker is allowed");
    }
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    new SharedWorker("a.js", "bar");
    ok(true, "SharedWorker is allowed");
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
