/* import-globals-from antitracking_head.js */

requestLongerTimeout(2);

AntiTracking.runTest(
  "DOM Cache Always Partition Storage and Storage Access API",
  async _ => {
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

    await caches.open("wow").then(
      _ => {
        ok(!shouldThrow, "DOM Cache can be used!");
      },
      _ => {
        ok(shouldThrow, "DOM Cache can be used!");
      }
    );

    await callRequestStorageAccess();

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
  [
    ["dom.caches.testing.enabled", true],
    ["privacy.partition.always_partition_third_party_non_cookie_storage", true],
  ],
  false,
  false
);
