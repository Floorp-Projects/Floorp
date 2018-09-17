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
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    try {
      new SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }

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

    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");
  },
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");

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
    new SharedWorker("a.js", "foo");
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

    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    await navigator.serviceWorker.register("empty.js").then(
      _ => { ok(false, "ServiceWorker cannot be used!"); },
      _ => { ok(true, "ServiceWorker cannot be used!"); });

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

    await navigator.serviceWorker.register("empty.js").then(
      reg => { ok(true, "ServiceWorker can be used!"); return reg; },
      _ => { ok(false, "ServiceWorker cannot be used! " + _); }).then(
      reg => reg.unregister(),
      _ => { ok(false, "unregister failed"); });
  },
  async _ => {
    await SpecialPowers.pushPrefEnv({"set": [
       ["dom.serviceWorkers.exemptFromPerDomainMax", true],
       ["dom.serviceWorkers.enabled", true],
       ["dom.serviceWorkers.testing.enabled", true],
    ]});

    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    await navigator.serviceWorker.register("empty.js").then(
      reg => { ok(true, "ServiceWorker can be used!"); return reg; },
      _ => { ok(false, "ServiceWorker cannot be used!"); }).then(
      reg => reg.unregister(),
      _ => { ok(false, "unregister failed"); });

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
    await navigator.serviceWorker.register("empty.js").then(
      reg => { ok(true, "ServiceWorker can be used!"); return reg; },
      _ => { ok(false, "ServiceWorker cannot be used!"); }).then(
      reg => reg.unregister(),
      _ => { ok(false, "unregister failed"); });
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
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    await caches.open("wow").then(
      _ => { ok(false, "DOM Cache cannot be used!"); },
      _ => { ok(true, "DOM Cache cannot be used!"); });

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

    await caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });
  },
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    await caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });

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
