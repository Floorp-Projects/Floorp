/* import-globals-from antitracking_head.js */

AntiTracking.runTest(
  "ServiceWorkers",
  async _ => {
    await navigator.serviceWorker
      .register("empty.js")
      .then(
        _ => {
          ok(false, "ServiceWorker cannot be used!");
        },
        _ => {
          ok(true, "ServiceWorker cannot be used!");
        }
      )
      .catch(e => ok(false, "Promise rejected: " + e));
  },
  null,
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.ipc.processCount", 1],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]
);
