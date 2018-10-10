requestLongerTimeout(4);

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
    await navigator.serviceWorker.register("empty.js").then(
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

AntiTracking.runTest("SharedWorkers and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    try {
      new SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    new SharedWorker("a.js", "bar");
    ok(true, "SharedWorker is allowed");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);

AntiTracking.runTest("ServiceWorkers and Storage Access API",
  async _ => {
    await SpecialPowers.pushPrefEnv({"set": [
       ["dom.serviceWorkers.exemptFromPerDomainMax", true],
       ["dom.serviceWorkers.enabled", true],
       ["dom.serviceWorkers.testing.enabled", true],
    ]});

    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await navigator.serviceWorker.register("empty.js").then(
      _ => { ok(false, "ServiceWorker cannot be used!"); },
      _ => { ok(true, "ServiceWorker cannot be used!"); }).
      catch(e => ok(false, "Promise rejected: " + e));

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    await navigator.serviceWorker.register("empty.js").then(
      reg => { ok(true, "ServiceWorker can be used!"); return reg; },
      _ => { ok(false, "ServiceWorker cannot be used! " + _); }).then(
      reg => reg.unregister(),
      _ => { ok(false, "unregister failed"); }).
      catch(e => ok(false, "Promise rejected: " + e));
  },
  async _ => {
    await SpecialPowers.pushPrefEnv({"set": [
       ["dom.serviceWorkers.exemptFromPerDomainMax", true],
       ["dom.serviceWorkers.enabled", true],
       ["dom.serviceWorkers.testing.enabled", true],
    ]});

    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await navigator.serviceWorker.register("empty.js").then(
      reg => { ok(true, "ServiceWorker can be used!"); return reg; },
      _ => { ok(false, "ServiceWorker cannot be used!"); }).then(
      reg => reg.unregister(),
      _ => { ok(false, "unregister failed"); }).
      catch(e => ok(false, "Promise rejected: " + e));

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    await navigator.serviceWorker.register("empty.js").then(
      reg => { ok(true, "ServiceWorker can be used!"); return reg; },
      _ => { ok(false, "ServiceWorker cannot be used!"); }).then(
      reg => reg.unregister(),
      _ => { ok(false, "unregister failed"); }).
      catch(e => ok(false, "Promise rejected: " + e));
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  [["dom.serviceWorkers.exemptFromPerDomainMax", true],
   ["dom.serviceWorkers.enabled", true],
   ["dom.serviceWorkers.testing.enabled", true]],
  false, false);

AntiTracking.runTest("DOM Cache and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await caches.open("wow").then(
      _ => { ok(false, "DOM Cache cannot be used!"); },
      _ => { ok(true, "DOM Cache cannot be used!"); });

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    await caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    await caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);
