// BroadcastChannel is not considered part of CookieJar. It's not allowed to
// communicate with other windows with different cookie jar settings.
"use strict";

CookiePolicyHelper.runTest("BroadcastChannel", {
  cookieJarAccessAllowed: async w => {
    new w.BroadcastChannel("hello");
    ok(true, "BroadcastChannel be used");
  },

  cookieJarAccessDenied: async w => {
    try {
      new w.BroadcastChannel("hello");
      ok(false, "BroadcastChannel cannot be used!");
    } catch (e) {
      ok(true, "BroadcastChannel cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
});

CookiePolicyHelper.runTest("BroadcastChannel in workers", {
  cookieJarAccessAllowed: async w => {
    function nonBlockingCode() {
      new BroadcastChannel("hello");
      postMessage(true);
    }

    let blob = new w.Blob([
      nonBlockingCode.toString() + "; nonBlockingCode();",
    ]);
    ok(blob, "Blob has been created");

    let blobURL = w.URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new w.Worker(blobURL);
    ok(worker, "Worker has been created");

    await new w.Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });
  },

  cookieJarAccessDenied: async w => {
    function blockingCode() {
      try {
        new BroadcastChannel("hello");
        postMessage(false);
      } catch (e) {
        postMessage(e.name == "SecurityError");
      }
    }

    let blob = new w.Blob([blockingCode.toString() + "; blockingCode();"]);
    ok(blob, "Blob has been created");

    let blobURL = w.URL.createObjectURL(blob);
    ok(blobURL, "Blob URL has been created");

    let worker = new w.Worker(blobURL);
    ok(worker, "Worker has been created");

    await new w.Promise((resolve, reject) => {
      worker.onmessage = function(e) {
        if (e) {
          resolve();
        } else {
          reject();
        }
      };
    });
  },
});
