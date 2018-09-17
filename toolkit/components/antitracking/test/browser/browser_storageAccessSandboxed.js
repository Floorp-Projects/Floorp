ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Storage Access API called in a sandboxed iframe",
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
  false, // run in normal window
  "allow-scripts allow-same-origin"
);

AntiTracking.runTest("Storage Access API called in a sandboxed iframe with" +
                     " allow-storage-access-by-user-activation",
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
    ok(!threw, "requestStorageAccess should be available");
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  true, // expect blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-storage-access-by-user-activation"
);

AntiTracking.runTest("Verify that sandboxed contexts don't get the saved permission",
  // blocking callback
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  false, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin"
);

AntiTracking.runTest("Verify that sandboxed contexts with" +
                     " allow-storage-access-by-user-activation get the" +
                     " saved permission",
  // blocking callback
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Has storage access");

    localStorage.foo = 42;
    ok(true, "LocalStorage can be used!");
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  false, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-storage-access-by-user-activation"
);

AntiTracking.runTest("Verify that private browsing contexts don't get the saved permission",
  // blocking callback
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
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

AntiTracking.runTest("Verify that non-sandboxed contexts get the" +
                     " saved permission",
  // blocking callback
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Has storage access");

    localStorage.foo = 42;
    ok(true, "LocalStorage can be used!");
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
  false // no blocking notifications
);
