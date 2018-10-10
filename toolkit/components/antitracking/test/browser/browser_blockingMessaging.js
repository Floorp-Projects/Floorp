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

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
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

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

AntiTracking.runTest("BroadcastChannel and Storage Access API",
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");
    try {
      new BroadcastChannel("hello");
      ok(false, "BroadcastChannel cannot be used!");
    } catch (e) {
      ok(true, "BroadcastChannel cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }

    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let p;
    try {
      p = document.requestStorageAccess();
    } finally {
      helper.destruct();
    }
    await p;

    hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Now has storage access");
    new BroadcastChannel("hello");
    ok(true, "BroadcastChannel can be used");
  },
  async _ => {
    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");
    new BroadcastChannel("hello");
    ok(true, "BroadcastChanneli can be used");

    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let p;
    try {
      p = document.requestStorageAccess();
    } finally {
      helper.destruct();
    }
    await p;

    hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Now has storage access");
    new BroadcastChannel("hello");
    ok(true, "BroadcastChannel can be used");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);

AntiTracking.runTest("BroadcastChannel in workers and Storage Access API",
  async _ => {
    function blockingCode() {
      try {
        new BroadcastChannel("hello");
        postMessage(false);
      } catch (e) {
        postMessage(e.name == "SecurityError");
      }
    }
    function nonBlockingCode() {
      new BroadcastChannel("hello");
      postMessage(true);
    }

    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    let blob = new Blob([blockingCode.toString() + "; blockingCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });

    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let p;
    try {
      p = document.requestStorageAccess();
    } finally {
      helper.destruct();
    }
    await p;

    hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Now has storage access");

    blob = new Blob([nonBlockingCode.toString() + "; nonBlockingCode();"]);
    ok(blob, "Blob has been created");

    blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });
  },
  async _ => {
    function nonBlockingCode() {
      new BroadcastChannel("hello");
      postMessage(true);
    }

    let hasAccess = await document.hasStorageAccess();
    ok(!hasAccess, "Doesn't yet have storage access");

    let blob = new Blob([nonBlockingCode.toString() + "; nonBlockingCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });

    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);

    let p;
    try {
      p = document.requestStorageAccess();
    } finally {
      helper.destruct();
    }
    await p;

    hasAccess = await document.hasStorageAccess();
    ok(hasAccess, "Now has storage access");

    // For non-tracking windows, calling the API is a no-op

    worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);
