ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Storage Access API called in a private window",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess();
    ok(!threw, "requestStorageAccess should not throw");
    ok(rejected, "requestStorageAccess shouldn't be available");
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  false, // no blocking notifications
  true, // run in private window
  null // iframe sandbox
);
