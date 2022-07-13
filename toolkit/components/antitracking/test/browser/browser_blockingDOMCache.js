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
  [
    ["dom.caches.testing.enabled", true],
    [
      "privacy.partition.always_partition_third_party_non_cookie_storage",
      false,
    ],
  ]
);
