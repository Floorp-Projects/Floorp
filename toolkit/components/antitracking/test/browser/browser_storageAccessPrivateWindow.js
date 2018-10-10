ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Storage Access API called in a private window",
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
    try {
      await p;
    } catch (e) {
      threw = true;
    }
    ok(threw, "requestStorageAccess shouldn't be available");
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  false, // no blocking notifications
  true, // run in private window
  null // iframe sandbox
);
