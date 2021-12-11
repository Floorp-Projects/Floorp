/* import-globals-from antitracking_head.js */

AntiTracking.runTestInNormalAndPrivateMode(
  "IndexedDB in workers",
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
    function nonBlockCode() {
      indexedDB.open("test", "1");
      postMessage(true);
    }

    let blob = new Blob([nonBlockCode.toString() + "; nonBlockCode();"]);
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
