/* import-globals-from antitracking_head.js */

if (AppConstants.MOZ_CODE_COVERAGE) {
  requestLongerTimeout(12);
} else {
  requestLongerTimeout(6);
}

AntiTracking.runTestInNormalAndPrivateMode(
  "BroadcastChannel",
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
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

AntiTracking.runTestInNormalAndPrivateMode(
  "BroadcastChannel in workers",
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
        if (e.data) {
          resolve();
        } else {
          reject();
        }
      };

      worker.onerror = function(e) {
        reject();
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
        if (e.data) {
          resolve();
        } else {
          reject();
        }
      };

      worker.onerror = function(e) {
        reject();
      };
    });
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

AntiTracking.runTestInNormalAndPrivateMode(
  "BroadcastChannel and Storage Access API",
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    try {
      new BroadcastChannel("hello");
      ok(false, "BroadcastChannel cannot be used!");
    } catch (e) {
      ok(true, "BroadcastChannel cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    if (
      [
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
      ].includes(
        SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior")
      )
    ) {
      try {
        new BroadcastChannel("hello");
        ok(false, "BroadcastChannel cannot be used!");
      } catch (e) {
        ok(true, "BroadcastChannel cannot be used!");
        is(e.name, "SecurityError", "We want a security error message.");
      }
    } else {
      new BroadcastChannel("hello");
      ok(true, "BroadcastChannel can be used");
    }
  },
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    new BroadcastChannel("hello");
    ok(true, "BroadcastChanneli can be used");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    new BroadcastChannel("hello");
    ok(true, "BroadcastChannel can be used");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  null,
  false,
  false
);

AntiTracking.runTestInNormalAndPrivateMode(
  "BroadcastChannel in workers and Storage Access API",
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

    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    let blob = new Blob([blockingCode.toString() + "; blockingCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e.data) {
          resolve();
        } else {
          reject();
        }
      };

      worker.onerror = function(e) {
        reject();
      };
    });

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    if (
      [
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
      ].includes(
        SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior")
      )
    ) {
      blob = new Blob([blockingCode.toString() + "; blockingCode();"]);
    } else {
      blob = new Blob([nonBlockingCode.toString() + "; nonBlockingCode();"]);
    }

    ok(blob, "Blob has been created");

    blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e.data) {
          resolve();
        } else {
          reject();
        }
      };

      worker.onerror = function(e) {
        reject();
      };
    });
  },
  async _ => {
    function nonBlockingCode() {
      new BroadcastChannel("hello");
      postMessage(true);
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    let blob = new Blob([nonBlockingCode.toString() + "; nonBlockingCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e.data) {
          resolve();
        } else {
          reject();
        }
      };

      worker.onerror = function(e) {
        reject();
      };
    });

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op

    worker = new Worker(blobURL);
    ok(worker, "Worker has been created");

    await new Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e.data) {
          resolve();
        } else {
          reject();
        }
      };

      worker.onerror = function(e) {
        reject();
      };
    });
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  null,
  false,
  false
);
