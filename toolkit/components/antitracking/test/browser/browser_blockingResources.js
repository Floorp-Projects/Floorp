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
  });

AntiTracking.runTest("localStorage",
  async _ => {
    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  async _ => {
    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  });

AntiTracking.runTest("sessionStorage",
  async _ => {
    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");
  },
  async _ => {
    sessionStorage.foo = 42;
    ok(true, "SessionStorage is always allowed");
  });

AntiTracking.runTest("SharedWorkers",
  async _ => {
    try {
      new SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  async _ => {
    new SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");
  });

AntiTracking.runTest("ServiceWorkers",
  async _ => {
    await navigator.serviceWorker.register("empty.js", { scope: "/" }).then(
      _ => { ok(false, "ServiceWorker cannot be used!"); },
      _ => { ok(true, "ServiceWorker cannot be used!"); });
  },
  null,
  [["dom.serviceWorkers.exemptFromPerDomainMax", true],
   ["dom.serviceWorkers.enabled", true],
   ["dom.serviceWorkers.testing.enabled", true]]);

AntiTracking.runTest("DOM Cache",
  async _ => {
    await caches.open("wow").then(
      _ => { ok(false, "DOM Cache cannot be used!"); },
      _ => { ok(true, "DOM Cache cannot be used!"); });
  },
  async _ => {
    await caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });
  });
