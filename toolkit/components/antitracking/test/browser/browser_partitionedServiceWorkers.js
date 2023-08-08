/* import-globals-from storageAccessAPIHelpers.js */

PartitionedStorageHelper.runTest(
  "ServiceWorkers - disable partitioning",
  async (win3rdParty, win1stParty, allowed) => {
    // Partitioned serviceWorkers are disabled in third-party context.
    await win3rdParty.navigator.serviceWorker.register("empty.js").then(
      _ => {
        ok(allowed, "Success: ServiceWorker cannot be used!");
      },
      _ => {
        ok(!allowed, "Failed: ServiceWorker cannot be used!");
      }
    );

    await win1stParty.navigator.serviceWorker.register("empty.js").then(
      _ => {
        ok(true, "Success: ServiceWorker should be available!");
      },
      _ => {
        ok(false, "Failed: ServiceWorker should be available!");
      }
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
    ["privacy.partition.serviceWorkers", false],
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - enable partitioning",
  async (win3rdParty, win1stParty, allowed) => {
    // Partitioned serviceWorkers are enabled in third-party context.
    await win3rdParty.navigator.serviceWorker.register("empty.js").then(
      _ => {
        ok(
          true,
          "Success: ServiceWorker should be available in third parties."
        );
      },
      _ => {
        ok(
          false,
          "Failed: ServiceWorker should be available in third parties."
        );
      }
    );

    await win1stParty.navigator.serviceWorker.register("empty.js").then(
      _ => {
        ok(true, "Success: ServiceWorker should be available!");
      },
      _ => {
        ok(false, "Failed: ServiceWorker should be available!");
      }
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - MatchAll",
  async (win3rdParty, win1stParty, allowed) => {
    if (!win1stParty.sw) {
      win1stParty.sw = await registerServiceWorker(win1stParty, "matchAll.js");
    }

    let msgPromise = new Promise(resolve => {
      win1stParty.navigator.serviceWorker.addEventListener("message", msg => {
        resolve(msg.data);
      });
    });

    win1stParty.sw.postMessage(win3rdParty.location.href);
    let msg = await msgPromise;

    // The service worker will always be partitioned. So, the first party window
    // won't have control on the third-party window.
    is(
      false,
      msg,
      "We won't have the 3rd party window controlled regardless of StorageAccess."
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - Partition ScriptContext",
  async (win3rdParty, win1stParty, allowed) => {
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

    // Set a script value to first-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      {
        type: "SetScriptValue",
        value: "1stParty",
      }
    );
    ok(res.result, "OK", "Set script value to first-party service worker.");

    // Set a script value to third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      {
        type: "SetScriptValue",
        value: "3rdParty",
      }
    );
    ok(res.result, "OK", "Set script value to third-party service worker.");

    // Get and check script value from the first-party service worker.
    res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      { type: "GetScriptValue" }
    );
    is(
      res.value,
      "1stParty",
      "The script value in first party window is correct"
    );

    // Get and check script value from the third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      { type: "GetScriptValue" }
    );

    is(
      res.value,
      "3rdParty",
      "The script value in third party window is correct"
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - Partition DOM Cache",
  async (win3rdParty, win1stParty, allowed) => {
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

    // Set DOM cache to first-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      {
        type: "SetCache",
        value: "1stParty",
      }
    );
    ok(res.result, "OK", "Set cache to first-party service worker.");

    // Set DOM cache to third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      {
        type: "SetCache",
        value: "3rdParty",
      }
    );
    ok(res.result, "OK", "Set cache to third-party service worker.");

    // Check DOM cache from the first-party service worker.
    res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      { type: "HasCache", value: "1stParty" }
    );
    ok(
      res.value,
      "The '1stParty' cache storage should exist for the first-party window."
    );
    res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      { type: "HasCache", value: "3rdParty" }
    );
    ok(
      !res.value,
      "The '3rdParty' cache storage should not exist for the first-party window."
    );

    // Check DOM cache from the third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      { type: "HasCache", value: "1stParty" }
    );

    ok(
      !res.value,
      "The '1stParty' cache storage should not exist for the third-party window."
    );

    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      { type: "HasCache", value: "3rdParty" }
    );

    ok(
      res.value,
      "The '3rdParty' cache storage should exist for the third-party window."
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - Partition IndexedDB",
  async (win3rdParty, win1stParty, allowed) => {
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

    // Set indexedDB value to first-party service worker.
    let res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      {
        type: "SetIndexedDB",
        value: "1stParty",
      }
    );
    ok(res.result, "OK", "Set cache to first-party service worker.");

    // Set indexedDB value to third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      {
        type: "SetIndexedDB",
        value: "3rdParty",
      }
    );
    ok(res.result, "OK", "Set cache to third-party service worker.");

    // Get and check indexedDB value from the first-party service worker.
    res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      { type: "GetIndexedDB" }
    );
    is(
      res.value,
      "1stParty",
      "The indexedDB value in first party window is correct"
    );

    // Get and check indexedDB from the third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      { type: "GetIndexedDB" }
    );

    is(
      res.value,
      "3rdParty",
      "The indexedDB value in third party window is correct"
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - Partition Intercept",
  async (win3rdParty, win1stParty, allowed) => {
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

    // Fetch a resource in the first-party window.
    await win1stParty.fetch("empty.js");

    // Check that only the first-party service worker gets fetch event.
    let res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      { type: "GetFetchURL" }
    );
    is(
      res.value,
      "https://not-tracking.example.com/browser/toolkit/components/antitracking/test/browser/empty.js",
      "The first-party service worker received fetch event."
    );
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      { type: "GetFetchURL" }
    );
    is(
      res.value,
      "",
      "The third-party service worker received no fetch event."
    );

    // Fetch a resource in the third-party window.
    await win3rdParty.fetch("empty.js");

    // Check if the fetch event only happens in third-party service worker.
    res = await sendAndWaitWorkerMessage(
      win1stParty.sw,
      win1stParty.navigator.serviceWorker,
      { type: "GetFetchURL" }
    );
    is(
      res.value,
      "",
      "The first-party service worker received no fetch event."
    );
    res = await sendAndWaitWorkerMessage(
      win3rdParty.sw,
      win3rdParty.navigator.serviceWorker,
      { type: "GetFetchURL" }
    );
    is(
      res.value,
      "https://not-tracking.example.com/browser/toolkit/components/antitracking/test/browser/empty.js",
      "The third-party service worker received fetch event."
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

// Bug1743236 - Verify the content process won't crash if we create a dedicated
// worker in a service worker controlled third-party page with Storage Access.
PartitionedStorageHelper.runTest(
  "ServiceWorkers - Create Dedicated Worker",
  async (win3rdParty, win1stParty, allowed) => {
    // We only do this test when the storage access is granted.
    if (!allowed) {
      return;
    }

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

    // Create a dedicated worker in first-party window.
    let firstPartyWorker = new win1stParty.Worker("dedicatedWorker.js");

    // Post a message to the dedicated worker and wait until the message circles
    // back.
    await new Promise(resolve => {
      firstPartyWorker.addEventListener("message", msg => {
        if (msg.data == "1stParty") {
          resolve();
        }
      });

      firstPartyWorker.postMessage("1stParty");
    });

    // Create a dedicated worker in third-party window.
    let thirdPartyWorker = new win3rdParty.Worker("dedicatedWorker.js");

    // Post a message to the dedicated worker and wait until the message circles
    // back.
    await new Promise(resolve => {
      thirdPartyWorker.addEventListener("message", msg => {
        if (msg.data == "3rdParty") {
          resolve();
        }
      });

      thirdPartyWorker.postMessage("3rdParty");
    });

    firstPartyWorker.terminate();
    thirdPartyWorker.terminate();
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

// Bug1768193 - Verify the parent process won't crash if we create a shared
// worker in a service worker controlled third-party page with Storage Access.
PartitionedStorageHelper.runTest(
  "ServiceWorkers - Create Shared Worker",
  async (win3rdParty, win1stParty, allowed) => {
    // We only do this test when the storage access is granted.
    if (!allowed) {
      return;
    }

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

    // Create a shared worker in third-party window.
    let thirdPartyWorker = new win3rdParty.SharedWorker("sharedWorker.js");

    // Post a message to the dedicated worker and wait until the message circles
    // back.
    await new Promise(resolve => {
      thirdPartyWorker.port.onmessage = msg => {
        resolve();
      };
      thirdPartyWorker.onerror = _ => {
        ok(false, "We should not be here");
        resolve();
      };
      thirdPartyWorker.port.postMessage("count");
    });

    thirdPartyWorker.port.postMessage("close");
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
    ["privacy.partition.serviceWorkers", true],
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - Private Browsing with partitioning disabled",
  async (win3rdParty, win1stParty, allowed) => {
    // Partitioned serviceWorkers are disabled in third-party context.
    ok(
      !win3rdParty.navigator.serviceWorker,
      "ServiceWorker should not be available"
    );
    ok(
      !win1stParty.navigator.serviceWorker,
      "ServiceWorker should not be available"
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
    ["privacy.partition.serviceWorkers", false],
  ],

  {
    runInPrivateWindow: true,
  }
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - Private Browsing with partitioning enabled",
  async (win3rdParty, win1stParty, allowed) => {
    // Partitioned serviceWorkers are disabled in third-party context.
    ok(
      !win3rdParty.navigator.serviceWorker,
      "ServiceWorker should not be available"
    );
    ok(
      !win1stParty.navigator.serviceWorker,
      "ServiceWorker should not be available"
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
    ["privacy.partition.serviceWorkers", true],
  ],

  {
    runInPrivateWindow: true,
  }
);
