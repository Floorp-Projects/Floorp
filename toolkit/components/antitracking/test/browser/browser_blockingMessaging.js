AntiTracking.runTest("BroadcastChannel",
  async _ => {
    try {
      new BroadcastChannel("hello");
      ok(false, "BroadcastChannel cannot be used!");
    } catch (e) {
      ok(true, "BroadcastChannel cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  async _ => {
    new BroadcastChannel("hello");
    ok(true, "BroadcastChannel be used");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

AntiTracking.runTest("BroadcastChannel in workers",
  async _ => {
    function blockingCode() {
      try {
        new BroadcastChannel("hello");
        postMessage(false);
      } catch (e) {
        postMessage(e.name == "SecurityError");
      }
    }

    let blob = new Blob([blockingCode.toString() + "; blockingCode();"]);
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
    function nonBlockingCode() {
      new BroadcastChannel("hello");
      postMessage(true);
    }

    let blob = new Blob([nonBlockingCode.toString() + "; nonBlockingCode();"]);
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
