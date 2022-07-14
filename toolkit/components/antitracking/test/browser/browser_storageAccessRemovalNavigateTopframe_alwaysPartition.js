/* import-globals-from antitracking_head.js */

AntiTracking.runTest(
  "Storage Access is removed when topframe navigates",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();
  },

  // non-blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess();
    ok(!threw, "requestStorageAccess should not throw");
    ok(!rejected, "requestStorageAccess should be available");
  },
  // cleanup function
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [["privacy.partition.always_partition_third_party_non_cookie_storage", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expected blocking notifications
  false, // run in normal window
  null, // no iframe sandbox
  "navigate-topframe", // access removal type
  // after-removal callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();
  }
);
