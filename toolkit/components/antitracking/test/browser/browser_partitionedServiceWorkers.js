/* import-globals-from partitionedstorage_head.js */

PartitionedStorageHelper.runTest(
  "ServiceWorkers",
  async (win3rdParty, win1stParty, allowed) => {
    // ServiceWorkers are not supported. Always blocked.
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
  ]
);

PartitionedStorageHelper.runTest(
  "ServiceWorkers - MatchAll",
  async (win3rdParty, win1stParty, allowed) => {
    if (!win1stParty.sw) {
      let reg = await win1stParty.navigator.serviceWorker.register(
        "matchAll.js"
      );
      if (reg.installing.state !== "activated") {
        await new Promise(resolve => {
          let w = reg.installing;
          w.addEventListener("statechange", function onStateChange() {
            if (w.state === "activated") {
              w.removeEventListener("statechange", onStateChange);
              win1stParty.sw = reg.active;
              resolve();
            }
          });
        });
      }
    }

    let msgPromise = new Promise(resolve => {
      win1stParty.navigator.serviceWorker.addEventListener("message", msg => {
        resolve(msg.data);
      });
    });

    win1stParty.sw.postMessage(win3rdParty.location.href);
    let msg = await msgPromise;

    is(allowed, msg, "We want to have the 3rd party window controlled.");
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
  ]
);
