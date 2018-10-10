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
    await noStorageAccessInitially();

    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");

    await callRequestStorageAccess();

    sessionStorage.foo = 42;
    ok(true, "SessionStorage is allowed after calling the storage access API too");
  },
  async _ => {
    await noStorageAccessInitially();

    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");

    await callRequestStorageAccess();

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
