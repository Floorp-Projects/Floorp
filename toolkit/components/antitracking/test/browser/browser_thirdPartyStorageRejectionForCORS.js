// This test works by setting up an exception for the tracker domain, which
// disables all the anti-tracking tests.

add_task(async _ => {
  PermissionTestUtils.add(
    "http://example.net",
    "cookie",
    Services.perms.ALLOW_ACTION
  );

  registerCleanupFunction(_ => {
    Services.perms.removeAll();
  });
});

AntiTracking._createTask({
  name: "Test that we don't store 3P cookies from non-anonymous CORS XHR",
  cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
  blockingByContentBlockingRTUI: false,
  allowList: false,
  thirdPartyPage: TEST_DOMAIN + TEST_PATH + "3rdParty.html",
  callback: async _ => {
    await new Promise(resolve => {
      const xhr = new XMLHttpRequest();
      xhr.open(
        "GET",
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/cookiesCORS.sjs?some;max-age=999999",
        true
      );
      xhr.withCredentials = true;
      xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      xhr.onreadystatechange = _ => {
        if (4 === xhr.readyState && 200 === xhr.status) {
          resolve();
        }
      };
      xhr.send();
    });
  },
  extraPrefs: [],
  expectedBlockingNotifications:
    Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
