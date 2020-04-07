"use strict";

CookiePolicyHelper.runTest("ServiceWorker", {
  prefs: [
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.ipc.processCount", 1],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ],

  cookieJarAccessAllowed: async w => {
    await w.navigator.serviceWorker
      .register("file_empty.js")
      .then(
        reg => {
          ok(true, "ServiceWorker can be used!");
          return reg;
        },
        _ => {
          ok(false, "ServiceWorker cannot be used! " + _);
        }
      )
      .then(
        reg => reg.unregister(),
        _ => {
          ok(false, "unregister failed");
        }
      )
      .catch(e => ok(false, "Promise rejected: " + e));
  },

  cookieJarAccessDenied: async w => {
    await w.navigator.serviceWorker
      .register("file_empty.js")
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
});
