/* import-globals-from antitracking_head.js */

requestLongerTimeout(2);

AntiTracking.runTest(
  "DOM Cache Always Partition Storage",
  async _ => {
    let effectiveCookieBehavior = SpecialPowers.isContentWindowPrivate(window)
      ? SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior.pbmode"
        )
      : SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior"
        );

    // caches.open still fails for cookieBehavior 2 (reject) and
    // 1 (reject for third parties), so we need to account for that.
    let shouldFail =
      effectiveCookieBehavior ==
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT ||
      (effectiveCookieBehavior ==
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN &&
        !document.location.href.includes("3rdPartyWO") &&
        !document.location.href.includes("3rdPartyUI"));

    await caches.open("wow").then(
      _ => {
        ok(!shouldFail, "DOM Cache can be used!");
      },
      _ => {
        ok(shouldFail, "DOM Cache can be used!");
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
  [
    ["dom.caches.testing.enabled", true],
    ["privacy.partition.always_partition_third_party_non_cookie_storage", true],
  ]
);
