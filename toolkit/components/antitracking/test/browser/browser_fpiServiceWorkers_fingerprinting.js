/* import-globals-from storageAccessAPIHelpers.js */

// This test ensures that a service worker for an exempted domain is exempted when it is
// in the first party context, and not exempted when it is in a third party context.

PartitionedStorageHelper.runTest(
  "ServiceWorkers - Check that RFP correctly is exempted and not exempted when FPI is enabled",
  async (win3rdParty, win1stParty, allowed) => {
    // Quickly unset and reset these prefs so we can get the real navigator.hardwareConcurrency
    // It can't be set enternally and then captured because this function gets stringified and then evaled in a new scope.
    await SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.firstParty.isolate", false],
        ["privacy.resistFingerprinting", false],
      ],
    });

    var SPOOFED_HW_CONCURRENCY = 2;
    var DEFAULT_HARDWARE_CONCURRENCY = navigator.hardwareConcurrency;

    await SpecialPowers.popPrefEnv();

    // Register service worker for the first-party window.
    if (!win1stParty.sw) {
      win1stParty.sw = await registerServiceWorker(
        win1stParty,
        "serviceWorker.js"
      );
    }

    // Register service worker for the third-party window.
    if (!win3rdParty.sw) {
      win3rdParty.sw = await registerServiceWorker(
        win3rdParty,
        "serviceWorker.js"
      );
    }

    // Check DOM cache from the first-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      { type: "GetHWConcurrency" }
    );
    console.info(
      "First Party, got: " +
        res.value +
        " Expected: " +
        DEFAULT_HARDWARE_CONCURRENCY
    );
    is(
      res.value,
      DEFAULT_HARDWARE_CONCURRENCY,
      "As a first party, HW Concurrency should not be spoofed"
    );

    // Check DOM cache from the third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      { type: "GetHWConcurrency" }
    );
    console.info(
      "Third Party, got: " + res.value + " Expected: " + SPOOFED_HW_CONCURRENCY
    );
    is(
      res.value,
      SPOOFED_HW_CONCURRENCY,
      "As a third party, HW Concurrency should be spoofed"
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
    ["privacy.firstParty.isolate", true],
    ["privacy.resistFingerprinting", true],
    ["privacy.resistFingerprinting.exemptedDomains", "*.example.com"],
  ]
);
