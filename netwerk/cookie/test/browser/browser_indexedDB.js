CookiePolicyHelper.runTest("IndexedDB", {
  cookieJarAccessAllowed: async _ => {
    content.indexedDB.open("test", "1");
    ok(true, "IDB should be allowed");
  },

  cookieJarAccessDenied: async _ => {
    try {
      content.indexedDB.open("test", "1");
      ok(false, "IDB should be blocked");
    } catch (e) {
      ok(true, "IDB should be blocked");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
});

CookiePolicyHelper.runTest("IndexedDB in workers", {
  cookieJarAccessAllowed: async _ => {
    function nonBlockCode() {
      indexedDB.open("test", "1");
      postMessage(true);
    }

    let blob = new content.Blob([nonBlockCode.toString() + "; nonBlockCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = content.URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new content.Worker(blobURL);
    ok(worker, "Worker has been created");

    await new content.Promise((resolve, reject) => {
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

  cookieJarAccessDenied: async _ => {
    function blockCode() {
      try {
        indexedDB.open("test", "1");
        postMessage(false);
      } catch (e) {
        postMessage(e.name == "SecurityError");
      }
    }

    let blob = new content.Blob([blockCode.toString() + "; blockCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = content.URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new content.Worker(blobURL);
    ok(worker, "Worker has been created");

    await new content.Promise((resolve, reject) => {
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
});
