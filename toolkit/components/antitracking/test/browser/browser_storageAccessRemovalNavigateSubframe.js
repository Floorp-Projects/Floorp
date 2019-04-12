/* import-globals-from antitracking_head.js */

AntiTracking.runTest("Storage Access is removed when subframe navigates",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();
  },

  // non-blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    if (allowListed) {
      await hasStorageAccessInitially();
    } else {
      await noStorageAccessInitially();
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess();
    ok(!threw, "requestStorageAccess should not throw");
    ok(!rejected, "requestStorageAccess should be available");
  },
  // cleanup function
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0, // no blocking notifications
  false, // run in normal window
  null, // no iframe sandbox
  "navigate-subframe", // access removal type
  // after-removal callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();
  }
);
