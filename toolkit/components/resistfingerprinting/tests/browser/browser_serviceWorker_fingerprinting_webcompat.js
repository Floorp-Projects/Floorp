/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from testHelpers.js */

runTestInFirstAndThirdPartyContexts(
  "ServiceWorkers - Ensure the fingerprinting WebCompat overrides the fingerprinting protection in third-party context.",
  async win => {
    let SPOOFED_HW_CONCURRENCY = 2;

    // Register service worker for the first-party window.
    if (!win.sw) {
      win.sw = await registerServiceWorker(win, "serviceWorker.js");
    }

    // Check navigator HW concurrency from the first-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win.sw,
      win.navigator.serviceWorker,
      { type: "GetHWConcurrency" }
    );
    is(
      res.value,
      SPOOFED_HW_CONCURRENCY,
      "HW Concurrency should be spoofed in the first-party context"
    );
  },

  async win => {
    // Quickly unset and reset these prefs so we can get the real navigator.hardwareConcurrency
    // It can't be set externally and then captured because this function gets stringified and
    // then evaled in a new scope.
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.fingerprintingProtection", false]],
    });

    var DEFAULT_HARDWARE_CONCURRENCY = navigator.hardwareConcurrency;

    await SpecialPowers.popPrefEnv();

    // Register service worker for the third-party window.
    if (!win.sw) {
      win.sw = await registerServiceWorker(win, "serviceWorker.js");
    }

    // Check navigator HW concurrency from the third-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win.sw,
      win.navigator.serviceWorker,
      { type: "GetHWConcurrency" }
    );
    is(
      res.value,
      DEFAULT_HARDWARE_CONCURRENCY,
      "HW Concurrency should not be spoofed in the third-party context"
    );
  },

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
    ["privacy.fingerprintingProtection", true],
    ["privacy.fingerprintingProtection.overrides", "+NavigatorHWConcurrency"],
    [
      "privacy.fingerprintingProtection.granularOverrides",
      JSON.stringify([
        {
          id: "1",
          last_modified: 1000000000000001,
          overrides: "-NavigatorHWConcurrency",
          firstPartyDomain: "example.net",
          thirdPartyDomain: "example.com",
        },
      ]),
    ],
  ]
);

runTestInFirstAndThirdPartyContexts(
  "ServiceWorkers - Ensure the fingerprinting WebCompat overrides the fingerprinting protection in third-party context with FPI enabled.",
  async win => {
    let SPOOFED_HW_CONCURRENCY = 2;

    // Register service worker for the first-party window.
    if (!win.sw) {
      win.sw = await registerServiceWorker(win, "serviceWorker.js");
    }

    // Check navigator HW concurrency from the first-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win.sw,
      win.navigator.serviceWorker,
      { type: "GetHWConcurrency" }
    );
    is(
      res.value,
      SPOOFED_HW_CONCURRENCY,
      "HW Concurrency should be spoofed in the first-party context"
    );
  },

  async win => {
    // Quickly unset and reset these prefs so we can get the real navigator.hardwareConcurrency
    // It can't be set enternally and then captured because this function gets stringified and
    // then evaled in a new scope.
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.fingerprintingProtection", false]],
    });

    var DEFAULT_HARDWARE_CONCURRENCY = navigator.hardwareConcurrency;

    await SpecialPowers.popPrefEnv();

    // Register service worker for the third-party window.
    if (!win.sw) {
      win.sw = await registerServiceWorker(win, "serviceWorker.js");
    }

    // Check navigator HW concurrency from the third-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win.sw,
      win.navigator.serviceWorker,
      { type: "GetHWConcurrency" }
    );
    is(
      res.value,
      DEFAULT_HARDWARE_CONCURRENCY,
      "HW Concurrency should not be spoofed in the third-party context"
    );
  },

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
    ["privacy.firstparty.isolate", true],
    ["privacy.fingerprintingProtection", true],
    ["privacy.fingerprintingProtection.overrides", "+NavigatorHWConcurrency"],
    [
      "privacy.fingerprintingProtection.granularOverrides",
      JSON.stringify([
        {
          id: "1",
          last_modified: 1000000000000001,
          overrides: "-NavigatorHWConcurrency",
          firstPartyDomain: "example.net",
          thirdPartyDomain: "example.com",
        },
      ]),
    ],
  ]
);
