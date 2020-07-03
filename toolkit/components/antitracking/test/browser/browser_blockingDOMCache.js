/* import-globals-from antitracking_head.js */

requestLongerTimeout(2);

AntiTracking.runTest(
  "DOM Cache",
  async _ => {
    await caches.open("wow").then(
      _ => {
        ok(false, "DOM Cache cannot be used!");
      },
      _ => {
        ok(true, "DOM Cache cannot be used!");
      }
    );
  },
  async _ => {
    await caches.open("wow").then(
      _ => {
        ok(true, "DOM Cache can be used!");
      },
      _ => {
        ok(false, "DOM Cache can be used!");
      }
    );
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [["dom.caches.testing.enabled", true]]
);

AntiTracking.runTest(
  "DOM Cache and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await caches.open("wow").then(
      _ => {
        ok(false, "DOM Cache cannot be used!");
      },
      _ => {
        ok(true, "DOM Cache cannot be used!");
      }
    );

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    let shouldThrow = [
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    ].includes(
      SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior")
    );

    await caches.open("wow").then(
      _ => {
        ok(!shouldThrow, "DOM Cache can be used!");
      },
      _ => {
        ok(shouldThrow, "DOM Cache can be used!");
      }
    );
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    await caches.open("wow").then(
      _ => {
        ok(true, "DOM Cache can be used!");
      },
      _ => {
        ok(false, "DOM Cache can be used!");
      }
    );

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    await caches.open("wow").then(
      _ => {
        ok(true, "DOM Cache can be used!");
      },
      _ => {
        ok(false, "DOM Cache can be used!");
      }
    );
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [["dom.caches.testing.enabled", true]],
  false,
  false
);
