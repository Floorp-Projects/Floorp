/* import-globals-from antitracking_head.js */

AntiTracking._createTask({
  name:
    "Test that after a storage access grant we have full first-party access",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: true,
  allowList: false,
  callback: async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await callRequestStorageAccess();

    const TRACKING_PAGE =
      "http://another-tracking.example.net/browser/browser/base/content/test/protectionsUI/trackingPage.html";
    async function runChecks(name) {
      let iframe = document.createElement("iframe");
      iframe.src = TRACKING_PAGE;
      document.body.appendChild(iframe);
      await new Promise(resolve => {
        iframe.onload = resolve;
      });

      await SpecialPowers.spawn(iframe, [name], name => {
        content.postMessage(name, "*");
      });

      await new Promise(resolve => {
        onmessage = e => {
          if (e.data == "done") {
            resolve();
          }
        };
      });
    }

    await runChecks("image");
  },
  extraPrefs: null,
  expectedBlockingNotifications:
    Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  thirdPartyPage: TEST_3RD_PARTY_PAGE_HTTP,
  errorMessageDomains: [
    "http://tracking.example.org",
    "http://tracking.example.org",
    "http://tracking.example.org",
    "http://tracking.example.org",
    "http://tracking.example.org",
  ],
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
