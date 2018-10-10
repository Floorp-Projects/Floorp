ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Storage Access API returns promises that maintain user activation",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess(dwu => {
      ok(dwu.isHandlingUserInput,
         "Promise handler must run as if we're handling user input");
    });
    ok(!threw, "requestStorageAccess should not throw");
    ok(!rejected, "requestStorageAccess should be available");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, // extra prefs
  false, // no window open test
  false // no user-interaction test
);
