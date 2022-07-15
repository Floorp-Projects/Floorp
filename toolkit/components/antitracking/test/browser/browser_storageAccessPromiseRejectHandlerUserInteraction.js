/* import-globals-from antitracking_head.js */

AntiTracking.runTest(
  "Storage Access API returns promises that do not maintain user activation for calling its reject handler",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess(() => {
      ok(
        !SpecialPowers.wrap(document).hasValidTransientUserGestureActivation,
        "Promise reject handler must not have user activation"
      );
    }, true);
    ok(!threw, "requestStorageAccess should not throw");
    ok(rejected, "requestStorageAccess should not be available");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [
    [
      "privacy.partition.always_partition_third_party_non_cookie_storage",
      false,
    ],
  ], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0, // expected blocking notifications
  false, // private window
  "allow-scripts allow-same-origin allow-popups" // iframe sandbox
);
