AntiTracking.runTest("localStorage",
  async _ => {
    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  async _ => {
    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

AntiTracking.runTest("localStorage and Storage Access API",
  async _ => {
    await noStorageAccessInitially();

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }

    await callRequestStorageAccess();

    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  },
  async _ => {
    await noStorageAccessInitially();

    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");

    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);
