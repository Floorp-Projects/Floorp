/* import-globals-from antitracking_head.js */

requestLongerTimeout(6);

AntiTracking.runTestInNormalAndPrivateMode(
  "IndexedDB in workers and Storage Access API",
  async _ => {
    function blockCode() {
      try {
        indexedDB.open("test", "1");
        postMessage(false);
      } catch (e) {
        postMessage(e.name == "SecurityError");
      }
    }
    function nonBlockCode() {
      indexedDB.open("test", "1");
      postMessage(true);
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

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

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    let effectiveCookieBehavior = SpecialPowers.isContentWindowPrivate(window)
      ? SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior.pbmode"
        )
      : SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior"
        );

    if (
      [
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
      ].includes(effectiveCookieBehavior)
    ) {
      blob = new Blob([blockCode.toString() + "; blockCode();"]);
    } else {
      blob = new Blob([nonBlockCode.toString() + "; nonBlockCode();"]);
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
    function nonBlockCode() {
      indexedDB.open("test", "1");
      postMessage(true);
    }

    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

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
