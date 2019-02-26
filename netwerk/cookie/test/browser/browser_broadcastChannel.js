// BroadcastChannel is not considered part of CookieJar. It's not allowed to
// communicate with other windows with different cookie settings.

CookiePolicyHelper.runTest("BroadcastChannel", {
  cookieJarAccessAllowed: async _ => {
    new content.BroadcastChannel("hello");
    ok(true, "BroadcastChannel be used");
  },

  cookieJarAccessDenied: async _ => {
    try {
      new content.BroadcastChannel("hello");
      ok(false, "BroadcastChannel cannot be used!");
    } catch (e) {
      ok(true, "BroadcastChannel cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  }
});

CookiePolicyHelper.runTest("BroadcastChannel in workers", {
  cookieJarAccessAllowed: async _ => {
    function nonBlockingCode() {
      new BroadcastChannel("hello");
      postMessage(true);
    }

    let blob = new content.Blob([nonBlockingCode.toString() + "; nonBlockingCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = content.URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new content.Worker(blobURL);
    ok(worker, "Worker has been created");

    await new content.Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });
  },

  cookieJarAccessDenied: async _ => {
    function blockingCode() {
      try {
        new BroadcastChannel("hello");
        postMessage(false);
      } catch (e) {
        postMessage(e.name == "SecurityError");
      }
    }

    let blob = new content.Blob([blockingCode.toString() + "; blockingCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = content.URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new content.Worker(blobURL);
    ok(worker, "Worker has been created");

    await new content.Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });
  }
});
