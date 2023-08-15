/* import-globals-from antitracking_head.js */

// This test ensures HasStorageAccess API returns the right value under different
// scenarios.

var settings = [
  // same-origin no-tracker
  {
    name: "Test whether same-origin non-tracker frame has storage access",
    topPage: TEST_TOP_PAGE_HTTPS,
    thirdPartyPage: TEST_DOMAIN_HTTPS + TEST_PATH + "3rdParty.html",
  },
  // 3rd-party no-tracker
  {
    name: "Test whether 3rd-party non-tracker frame has storage access",
    topPage: TEST_TOP_PAGE_HTTPS,
    thirdPartyPage: TEST_4TH_PARTY_PAGE_HTTPS,
  },
  // 3rd-party no-tracker with permission
  {
    name: "Test whether 3rd-party non-tracker frame has storage access when storage permission is granted before",
    topPage: TEST_TOP_PAGE_HTTPS,
    thirdPartyPage: TEST_4TH_PARTY_PAGE_HTTPS,
    setup: () => {
      let type = "3rdPartyFrameStorage^https://not-tracking.example.com";
      let permission = Services.perms.ALLOW_ACTION;
      let expireType = Services.perms.EXPIRE_SESSION;
      PermissionTestUtils.add(
        TEST_DOMAIN_HTTPS,
        type,
        permission,
        expireType,
        0
      );

      registerCleanupFunction(_ => {
        Services.perms.removeAll();
      });
    },
  },
  // 3rd-party tracker
  {
    name: "Test whether 3rd-party tracker frame has storage access",
    topPage: TEST_TOP_PAGE_HTTPS,
    thirdPartyPage: TEST_3RD_PARTY_PAGE,
  },
  // 3rd-party tracker with permission
  {
    name: "Test whether 3rd-party tracker frame has storage access when storage access permission is granted before",
    topPage: TEST_TOP_PAGE_HTTPS,
    thirdPartyPage: TEST_3RD_PARTY_PAGE,
    setup: () => {
      let type = "3rdPartyFrameStorage^https://example.org";
      let permission = Services.perms.ALLOW_ACTION;
      let expireType = Services.perms.EXPIRE_SESSION;
      PermissionTestUtils.add(
        TEST_DOMAIN_HTTPS,
        type,
        permission,
        expireType,
        0
      );

      registerCleanupFunction(_ => {
        Services.perms.removeAll();
      });
    },
  },
  // same-site 3rd-party tracker
  {
    name: "Test whether same-site 3rd-party tracker frame has storage access",
    topPage: TEST_TOP_PAGE_HTTPS,
    thirdPartyPage: TEST_ANOTHER_3RD_PARTY_PAGE_HTTPS,
  },
  // same-origin 3rd-party tracker
  {
    name: "Test whether same-origin 3rd-party tracker frame has storage access",
    topPage: TEST_ANOTHER_3RD_PARTY_DOMAIN_HTTPS + TEST_PATH + "page.html",
    thirdPartyPage: TEST_ANOTHER_3RD_PARTY_PAGE_HTTPS,
  },
  // Insecure 3rd-party tracker
  {
    name: "Test whether insecure 3rd-party tracker frame has storage access",
    topPage: TEST_TOP_PAGE + TEST_PATH + "page.html",
    thirdPartyPage: TEST_3RD_PARTY_PAGE_HTTP,
  },
];

var testCases = [
  {
    behavior: BEHAVIOR_ACCEPT, // 0
    hasStorageAccess: [
      true /* same-origin non-tracker */,
      true /* 3rd-party non-tracker */,
      true /* 3rd-party non-tracker with permission */,
      true /* 3rd-party tracker */,
      true /* 3rd-party tracker with permission */,
      true /* same-site tracker */,
      true /* same-origin tracker */,
      true /* Insecure context */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT_FOREIGN, // 1
    hasStorageAccess: [
      true /* same-origin non-tracker */,
      false /* 3rd-party non-tracker */,
      false /* 3rd-party non-tracker with permission */,
      false /* 3rd-party tracker */,
      false /* 3rd-party tracker with permission */,
      true /* same-site tracker */,
      true /* same-origin tracker */,
      false /* Insecure context */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT, // 2
    hasStorageAccess: [
      false /* same-origin non-tracker */,
      false /* 3rd-party non-tracker */,
      false /* 3rd-party non-tracker with permission */,
      false /* 3rd-party tracker */,
      false /* 3rd-party tracker with permission */,
      false /* same-site tracker */,
      false /* same-origin tracker */,
      false /* Insecure context */,
    ],
  },
  {
    behavior: BEHAVIOR_LIMIT_FOREIGN, // 3
    hasStorageAccess: [
      true /* same-origin non-tracker */,
      false /* 3rd-party non-tracker */,
      false /* 3rd-party non-tracker with permission */,
      false /* 3rd-party tracker */,
      false /* 3rd-party tracker with permission */,
      true /* same-site tracker */,
      true /* same-origin tracker */,
      false /* Insecure context */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT_TRACKER, // 4
    hasStorageAccess: [
      true /* same-origin non-tracker */,
      true /* 3rd-party non-tracker */,
      true /* 3rd-party non-tracker with permission */,
      false /* 3rd-party tracker */,
      false /* 3rd-party tracker with permission */,
      true /* same-site tracker */,
      true /* same-origin tracker */,
      false /* Insecure context */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN, // 5
    hasStorageAccess: [
      true /* same-origin non-tracker */,
      false /* 3rd-party non-tracker */,
      false /* 3rd-party non-tracker with permission */,
      false /* 3rd-party tracker */,
      false /* 3rd-party tracker with permission */,
      true /* same-site tracker */,
      true /* same-origin tracker */,
      false /* Insecure context */,
    ],
  },
];

(function () {
  settings.forEach(setting => {
    if (setting.setup) {
      add_task(async _ => {
        setting.setup();
      });
    }

    testCases.forEach(test => {
      let callback = test.hasStorageAccess[settings.indexOf(setting)]
        ? async _ => {
            /* import-globals-from storageAccessAPIHelpers.js */
            await hasStorageAccessInitially();
          }
        : async _ => {
            /* import-globals-from storageAccessAPIHelpers.js */
            await noStorageAccessInitially();
          };

      AntiTracking._createTask({
        name: setting.name,
        cookieBehavior: test.behavior,
        allowList: false,
        callback,
        extraPrefs: [
          [
            "privacy.partition.always_partition_third_party_non_cookie_storage",
            false,
          ],
        ],
        expectedBlockingNotifications: 0,
        runInPrivateWindow: false,
        iframeSandbox: null,
        accessRemoval: null,
        callbackAfterRemoval: null,
        topPage: setting.topPage,
        thirdPartyPage: setting.thirdPartyPage,
      });
    });

    add_task(async _ => {
      await new Promise(resolve => {
        Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
          resolve()
        );
      });
    });
  });
})();
