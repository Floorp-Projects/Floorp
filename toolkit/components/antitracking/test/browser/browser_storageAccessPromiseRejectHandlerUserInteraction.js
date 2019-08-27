/* import-globals-from antitracking_head.js */

AntiTracking.runTest(
  "Storage Access API returns promises that maintain user activation for calling its reject handler",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess(dwu => {
      ok(
        dwu.isHandlingUserInput,
        "Promise reject handler must run as if we're handling user input"
      );
    }, true);
    ok(!threw, "requestStorageAccess should not throw");
    ok(rejected, "requestStorageAccess should not be available");
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
  null, // extra prefs
  false, // no window open test
  false, // no user-interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expected blocking notifications
  false, // private window
  "allow-scripts allow-same-origin allow-popups" // iframe sandbox
);
