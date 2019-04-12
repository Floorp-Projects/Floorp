/* import-globals-from antitracking_head.js */

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

    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let promise;
    try {
      promise = document.hasStorageAccess();
    } finally {
      helper.destruct();
    }
    await promise.then(_ => {
      ok(dwu.isHandlingUserInput,
         "Promise handler must run as if we're handling user input");
    });
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
