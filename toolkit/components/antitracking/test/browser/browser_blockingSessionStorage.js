AntiTracking.runTest("sessionStorage",
  async _ => {
    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");
  },
  async _ => {
    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  [],
  true,
  true,
  false);

AntiTracking.runTest("sessionStorage and Storage Access API",
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");

    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let p;
    try {
      p = document.requestStorageAccess();
    } finally {
      helper.destruct();
    }
    await p;

    hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Now has storage access");

    sessionStorage.foo = 42;
    ok(true, "SessionStorage is allowed after calling the storage access API too");
  },
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");

    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let p;
    try {
      p = document.requestStorageAccess();
    } finally {
      helper.destruct();
    }
    await p;

    hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Now has storage access");

    // For non-tracking windows, calling the API is a no-op
    sessionStorage.foo = 42;
    ok(true, "SessionStorage is allowed after calling the storage access API too");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);
