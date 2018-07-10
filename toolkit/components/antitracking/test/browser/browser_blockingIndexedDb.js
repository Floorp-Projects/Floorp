AntiTracking.runTest("IndexedDB",
  // blocking callback
  async _ => {
    try {
      indexedDB.open("test", "1");
      ok(false, "IDB should be blocked");
    } catch (e) {
      ok(true, "IDB should be blocked");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  // non-blocking callback
  async _ => {
    indexedDB.open("test", "1");
    ok(true, "IDB should be allowed");
  },
  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

AntiTracking.runTest("IndexedDB in workers",
  async _ => {
    function blockCode() {
      try {
        indexedDB.open("test", "1");
        postMessage(false);
      } catch (e) {
        postMessage(e.name == "SecurityError");
      }
    }

    let blob = new Blob([blockCode.toString() + "; blockCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise(resolve => {
      worker.onmessage = function(e) {
        resolve();
      };
    });
  },
  async _ => {
    function nonBlockCode() {
      indexedDB.open("test", "1");
      postMessage(false);
    }

    let blob = new Blob([nonBlockCode.toString() + "; nonBlockCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise(resolve => {
      worker.onmessage = function(e) {
        resolve();
      };
    });
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });
