/* import-globals-from storageprincipal_head.js */

StoragePrincipalHelper.runTest("SharedWorkers",
  async (win3rdParty, win1stParty, allowed) => {
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
        ok(!allowed, "We should be here only if the SharedWorker is partitioned");
        is(e.data, 1, "We expected 1 connection for 3rd party SharedWorker");
        resolve();
      };
      sh3.onerror = _ => {
        ok(allowed, "We should be here only if the SharedWorker is not partitioned");
        resolve();
      };
      sh3.port.postMessage("count");
    });

    sh1.port.postMessage("close");
    sh3.port.postMessage("close");
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });
