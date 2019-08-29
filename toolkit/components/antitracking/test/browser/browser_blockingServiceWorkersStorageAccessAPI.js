/* import-globals-from antitracking_head.js */

requestLongerTimeout(2);

AntiTracking.runTest(
  "ServiceWorkers and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await navigator.serviceWorker
      .register("empty.js")
      .then(
        _ => {
          ok(false, "ServiceWorker cannot be used!");
        },
        _ => {
          ok(true, "ServiceWorker cannot be used!");
        }
      )
      .catch(e => ok(false, "Promise rejected: " + e));

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
      await navigator.serviceWorker
        .register("empty.js")
        .then(
          _ => {
            ok(false, "ServiceWorker cannot be used!");
          },
          _ => {
            ok(true, "ServiceWorker cannot be used!");
          }
        )
        .catch(e => ok(false, "Promise rejected: " + e));
    } else {
      await navigator.serviceWorker
        .register("empty.js")
        .then(
          reg => {
            ok(true, "ServiceWorker can be used!");
            return reg;
          },
          _ => {
            ok(false, "ServiceWorker cannot be used! " + _);
          }
        )
        .then(
          reg => reg.unregister(),
          _ => {
            ok(false, "unregister failed");
          }
        )
        .catch(e => ok(false, "Promise rejected: " + e));
    }
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    if (allowListed) {
      await hasStorageAccessInitially();
    } else {
      await noStorageAccessInitially();
    }

    await navigator.serviceWorker
      .register("empty.js")
      .then(
        reg => {
          ok(true, "ServiceWorker can be used!");
          return reg;
        },
        _ => {
          ok(false, "ServiceWorker cannot be used!");
        }
      )
      .then(
        reg => reg.unregister(),
        _ => {
          ok(false, "unregister failed");
        }
      )
      .catch(e => ok(false, "Promise rejected: " + e));

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    await navigator.serviceWorker
      .register("empty.js")
      .then(
        reg => {
          ok(true, "ServiceWorker can be used!");
          return reg;
        },
        _ => {
          ok(false, "ServiceWorker cannot be used!");
        }
      )
      .then(
        reg => reg.unregister(),
        _ => {
          ok(false, "unregister failed");
        }
      )
      .catch(e => ok(false, "Promise rejected: " + e));
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.ipc.processCount", 1],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ],
  false,
  false
);
