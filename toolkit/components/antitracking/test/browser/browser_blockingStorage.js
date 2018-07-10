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
  });
