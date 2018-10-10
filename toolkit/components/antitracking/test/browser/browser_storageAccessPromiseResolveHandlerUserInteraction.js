ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Storage Access API returns promises that maintain user activation",
  // blocking callback
  async _ => {
    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let p;
    let threw = false;
    try {
      p = document.requestStorageAccess();
    } catch (e) {
      threw = true;
    } finally {
      helper.destruct();
    }
    ok(!threw, "requestStorageAccess should not throw");
    threw = false;
    await p.then(() => {
    });
    threw = false;
    try {
      await p.then(() => {
        ok(dwu.isHandlingUserInput,
           "Promise handler must run as if we're handling user input");
      });
    } catch (e) {
      threw = true;
    }
    ok(!threw, "requestStorageAccess should be available");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  true, // expect blocking notifications
  false, // run in normal window
  null // iframe sandbox
);
