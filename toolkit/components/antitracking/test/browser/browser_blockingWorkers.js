AntiTracking.runTest("SharedWorkers",
  async _ => {
    try {
      new SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  async _ => {
    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

AntiTracking.runTest("ServiceWorkers",
  async _ => {
    await navigator.serviceWorker.register("empty.js", { scope: "/" }).then(
      _ => { ok(false, "ServiceWorker cannot be used!"); },
      _ => { ok(true, "ServiceWorker cannot be used!"); });
  },
  null,
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  [["dom.serviceWorkers.exemptFromPerDomainMax", true],
   ["dom.serviceWorkers.enabled", true],
   ["dom.serviceWorkers.testing.enabled", true]]);

AntiTracking.runTest("DOM Cache",
  async _ => {
    await caches.open("wow").then(
      _ => { ok(false, "DOM Cache cannot be used!"); },
      _ => { ok(true, "DOM Cache cannot be used!"); });
  },
  async _ => {
    await caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });
