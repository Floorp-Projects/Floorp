/* import-globals-from partitionedstorage_head.js */

PartitionedStorageHelper.runTestInNormalAndPrivateMode(
  "SharedWorkers",
  async (win3rdParty, win1stParty, allowed) => {
    // This test fails if run with an HTTPS 3rd-party URL because the shared worker
    // which would start from the window opened from 3rdPartyStorage.html will become
    // secure context and per step 11.4.3 of
    // https://html.spec.whatwg.org/multipage/workers.html#dom-sharedworker attempting
    // to run the SharedWorker constructor would emit an error event.
    is(
      win3rdParty.location.protocol,
      "http:",
      "Our 3rd party URL shouldn't be HTTPS"
    );

    let sh1 = new win1stParty.SharedWorker("sharedWorker.js");
    await new Promise(resolve => {
      sh1.port.onmessage = e => {
        is(e.data, 1, "We expected 1 connection");
        resolve();
      };
      sh1.port.postMessage("count");
    });

    let sh3 = new win3rdParty.SharedWorker("sharedWorker.js");
    await new Promise(resolve => {
      sh3.port.onmessage = e => {
        is(
          e.data,
          allowed ? 2 : 1,
          `We expected ${allowed ? 2 : 1} connection for 3rd party SharedWorker`
        );
        resolve();
      };
      sh3.onerror = _ => {
        ok(false, "We should not be here");
        resolve();
      };
      sh3.port.postMessage("count");
    });

    sh1.port.postMessage("close");
    sh3.port.postMessage("close");
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

PartitionedStorageHelper.runPartitioningTestInNormalAndPrivateMode(
  "Partitioned tabs - SharedWorker",

  // getDataCallback
  async win => {
    win.sh = new win.SharedWorker("partitionedSharedWorker.js");
    return new Promise(resolve => {
      win.sh.port.onmessage = e => {
        resolve(e.data);
      };
      win.sh.port.postMessage({ what: "get" });
    });
  },

  // addDataCallback
  async (win, value) => {
    win.sh = new win.SharedWorker("partitionedSharedWorker.js");
    win.sh.port.postMessage({ what: "put", value });
    return true;
  },

  // cleanup
  async _ => {}
);
