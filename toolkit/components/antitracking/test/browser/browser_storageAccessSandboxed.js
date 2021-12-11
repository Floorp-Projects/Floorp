/* import-globals-from antitracking_head.js */
/* import-globals-from storageAccessAPIHelpers.js */

AntiTracking.runTest(
  "Storage Access API called in a sandboxed iframe",
  // blocking callback
  async _ => {
    let [threw, rejected] = await callRequestStorageAccess();
    ok(!threw, "requestStorageAccess should not throw");
    ok(rejected, "requestStorageAccess shouldn't be available");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    // Only clear the user-interaction permissions for the tracker here so that
    // the next test has a clean slate.
    await new Promise(resolve => {
      Services.clearData.deleteDataFromHost(
        Services.io.newURI(TEST_3RD_PARTY_DOMAIN).host,
        true,
        Ci.nsIClearDataService.CLEAR_PERMISSIONS,
        value => resolve()
      );
    });
  },
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups"
);

AntiTracking.runTest(
  "Exception List can work in a sandboxed iframe",
  // blocking callback
  async _ => {
    await hasStorageAccessInitially();

    try {
      await navigator.serviceWorker.register("empty.js");

      ok(
        true,
        "ServiceWorker can be registered in allowlisted sandboxed iframe!"
      );
    } catch (e) {
      info("Promise rejected: " + e);
      ok(
        false,
        "ServiceWorker should be able to be registered in allowlisted sandboxed iframe"
      );
    }
  },

  null, // non-blocking callback
  null, // cleanup function
  [
    ["dom.storage_access.enabled", true],
    [
      "privacy.restrict3rdpartystorage.skip_list",
      "http://example.net,https://tracking.example.org",
    ],
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups"
);

AntiTracking.runTest(
  "Storage Access API called in a sandboxed iframe with" +
    " allow-storage-access-by-user-activation",
  // blocking callback
  async _ => {
    await noStorageAccessInitially();

    let [threw, rejected] = await callRequestStorageAccess();
    ok(!threw, "requestStorageAccess should not throw");
    ok(!rejected, "requestStorageAccess should be available");
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups allow-storage-access-by-user-activation"
);

AntiTracking.runTest(
  "Verify that sandboxed contexts don't get the saved permission",
  // blocking callback
  async _ => {
    await noStorageAccessInitially();

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups"
);

AntiTracking.runTest(
  "Verify that sandboxed contexts with" +
    " allow-storage-access-by-user-activation get the" +
    " saved permission",
  // blocking callback
  async _ => {
    await hasStorageAccessInitially();

    localStorage.foo = 42;
    ok(true, "LocalStorage can be used!");
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups allow-storage-access-by-user-activation"
);

AntiTracking.runTest(
  "Verify that private browsing contexts don't get the saved permission",
  // blocking callback
  async _ => {
    await noStorageAccessInitially();

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  true, // run in private window
  null // iframe sandbox
);

AntiTracking.runTest(
  "Verify that non-sandboxed contexts get the saved permission",
  // blocking callback
  async _ => {
    await hasStorageAccessInitially();

    localStorage.foo = 42;
    ok(true, "LocalStorage can be used!");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0 // no blocking notifications
);
