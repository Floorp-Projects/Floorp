// This test works by setting up an exception for the tracker domain, which
// disables all the anti-tracking tests.

/* import-globals-from antitracking_head.js */

add_task(async _ => {
  Services.perms.add(Services.io.newURI("https://tracking.example.org"),
                     "cookie", Services.perms.DENY_ACTION);
  Services.perms.add(Services.io.newURI("https://tracking.example.com"),
                     "cookie", Services.perms.DENY_ACTION);

  registerCleanupFunction(_ => {
    Services.perms.removeAll();
  });
});

AntiTracking._createTask({
  name: "Test that we do honour a cookie permission for nested windows",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: true,
  allowList: false,
  callback: async _ => {
    document.cookie = "name=value";
    ok(document.cookie == "", "All is blocked");
  },
  extraPrefs: null,
  expectedBlockingNotifications: Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
});
