requestLongerTimeout(2);

AntiTracking.runTest("ServiceWorkers",
  async _ => {
    await navigator.serviceWorker.register("empty.js").then(
      _ => { ok(false, "ServiceWorker cannot be used!"); },
      _ => { ok(true, "ServiceWorker cannot be used!"); }).
      catch(e => ok(false, "Promise rejected: " + e));
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
