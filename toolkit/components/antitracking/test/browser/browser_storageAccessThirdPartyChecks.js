const APS_PREF =
  "privacy.partition.always_partition_third_party_non_cookie_storage";

AntiTracking._createTask({
  name: "Test that after a storage access grant we have full first-party access",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: true,
  allowList: false,
  callback: async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await callRequestStorageAccess();

    const TRACKING_PAGE =
      "https://another-tracking.example.net/browser/toolkit/components/antitracking/test/browser/trackingPage.html";
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
  extraPrefs: [[APS_PREF, false]],
  expectedBlockingNotifications:
    Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  thirdPartyPage: TEST_3RD_PARTY_PAGE,
  errorMessageDomains: [
    "https://tracking.example.org",
    "https://tracking.example.org",
    "https://tracking.example.org",
  ],
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

AntiTracking._createTask({
  name: "Test that we never grant access to cookieBehavior=2",
  cookieBehavior: BEHAVIOR_REJECT,
  allowList: false,
  callback: async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await callRequestStorageAccess(null, true);
  },
  extraPrefs: [[APS_PREF, false]],
  expectedBlockingNotifications: 0,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  thirdPartyPage: TEST_3RD_PARTY_PAGE,
  errorMessageDomains: [
    "https://tracking.example.org",
    "https://tracking.example.org",
  ],
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

AntiTracking._createTask({
  name: "Test that we never grant access to cookieBehavior=3",
  cookieBehavior: BEHAVIOR_LIMIT_FOREIGN,
  allowList: false,
  callback: async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await callRequestStorageAccess(null, true);
  },
  extraPrefs: [[APS_PREF, false]],
  expectedBlockingNotifications: 0,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  thirdPartyPage: TEST_3RD_PARTY_PAGE,
  errorMessageDomains: [
    "https://tracking.example.org",
    "https://tracking.example.org",
  ],
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
