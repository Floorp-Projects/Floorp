// This test ensures HasStorageAccess API returns the right value under different
// scenarios.

/* import-globals-from antitracking_head.js */

var settings = [
  // same-origin no-tracker
  {
    name: "Test whether same-origin non-tracker frame has storage access",
    topPage: TEST_TOP_PAGE,
    thirdPartyPage: TEST_DOMAIN + TEST_PATH + "3rdParty.html",
  },
  // 3rd-party no-tracker
  {
    name: "Test whether 3rd-party non-tracker frame has storage access",
    topPage: TEST_TOP_PAGE,
    thirdPartyPage: TEST_4TH_PARTY_PAGE,
  },
  // 3rd-party no-tracker with permission
  {
    name:
      "Test whether 3rd-party non-tracker frame has storage access when storage permission is granted before",
    topPage: TEST_TOP_PAGE,
    thirdPartyPage: TEST_4TH_PARTY_PAGE,
    setup: () => {
      let type = "3rdPartyStorage^http://not-tracking.example.com";
      let permission = Services.perms.ALLOW_ACTION;
      let expireType = Services.perms.EXPIRE_SESSION;
      PermissionTestUtils.add(TEST_DOMAIN, type, permission, expireType, 0);

      registerCleanupFunction(_ => {
        Services.perms.removeAll();
      });
    },
  },
  // 3rd-party tracker
  {
    name: "Test whether 3rd-party tracker frame has storage access",
    topPage: TEST_TOP_PAGE,
    thirdPartyPage: TEST_3RD_PARTY_PAGE,
  },
  // 3rd-party tracker with permission
  {
    name:
      "Test whether 3rd-party tracker frame has storage access when storage access permission is granted before",
    topPage: TEST_TOP_PAGE,
    thirdPartyPage: TEST_3RD_PARTY_PAGE,
    setup: () => {
      let type = "3rdPartyStorage^https://tracking.example.org";
      let permission = Services.perms.ALLOW_ACTION;
      let expireType = Services.perms.EXPIRE_SESSION;
      PermissionTestUtils.add(TEST_DOMAIN, type, permission, expireType, 0);

      registerCleanupFunction(_ => {
        Services.perms.removeAll();
      });
    },
  },
  // same-site 3rd-party tracker
  {
    name: "Test whether same-site 3rd-party tracker frame has storage access",
    topPage: TEST_TOP_PAGE,
    thirdPartyPage: TEST_ANOTHER_3RD_PARTY_PAGE,
  },
  // same-origin 3rd-party tracker
  {
    name: "Test whether same-origin 3rd-party tracker frame has storage access",
    topPage: TEST_ANOTHER_3RD_PARTY_DOMAIN + TEST_PATH + "page.html",
    thirdPartyPage: TEST_ANOTHER_3RD_PARTY_PAGE,
  },
];

const allBlocked = Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL;
const foreignBlocked = Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN;
const trackerBlocked = Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER;

var testCases = [
  {
    behavior: BEHAVIOR_ACCEPT, // 0
    cases: [
      [true] /* same-origin non-tracker */,
      [true] /* 3rd-party non-tracker */,
      [true] /* 3rd-party non-tracker with permission */,
      [true] /* 3rd-party tracker */,
      [true] /* 3rd-party tracker with permission */,
      [true] /* same-site tracker */,
      [true] /* same-origin tracker */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT_FOREIGN, // 1
    cases: [
      [true] /* same-origin non-tracker */,
      [false, foreignBlocked] /* 3rd-party non-tracker */,
      [
        SpecialPowers.Services.prefs.getBoolPref(
          "network.cookie.rejectForeignWithExceptions.enabled"
        ),
        foreignBlocked,
      ] /* 3rd-party tracker with permission */,
      [false, foreignBlocked] /* 3rd-party tracker */,
      [
        SpecialPowers.Services.prefs.getBoolPref(
          "network.cookie.rejectForeignWithExceptions.enabled"
        ),
        foreignBlocked,
      ] /* 3rd-party non-tracker with permission */,
      [true] /* same-site tracker */,
      [true] /* same-origin tracker */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT, // 2
    cases: [
      [false, allBlocked] /* same-origin non-tracker */,
      [false, allBlocked] /* 3rd-party non-tracker */,
      [false, allBlocked] /* 3rd-party non-tracker with permission */,
      [false, allBlocked] /* 3rd-party tracker */,
      [false, allBlocked] /* 3rd-party tracker with permission */,
      [false, allBlocked] /* same-site tracker */,
      [false, allBlocked] /* same-origin tracker */,
    ],
  },
  {
    behavior: BEHAVIOR_LIMIT_FOREIGN, // 3
    cases: [
      [true] /* same-origin non-tracker */,
      [false, foreignBlocked] /* 3rd-party non-tracker */,
      [false, foreignBlocked] /* 3rd-party non-tracker with permission */,
      [false, foreignBlocked] /* 3rd-party tracker */,
      [false, foreignBlocked] /* 3rd-party tracker with permission */,
      [true] /* same-site tracker */,
      [true] /* same-origin tracker */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT_TRACKER, // 4
    cases: [
      [true] /* same-origin non-tracker */,
      [true] /* 3rd-party non-tracker */,
      [true] /* 3rd-party non-tracker with permission */,
      [false, trackerBlocked] /* 3rd-party tracker */,
      [true] /* 3rd-party tracker with permission */,
      [true] /* same-site tracker */,
      [true] /* same-origin tracker */,
    ],
  },
  {
    behavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN, // 5
    cases: [
      [true] /* same-origin non-tracker */,
      [false] /* 3rd-party non-tracker */,
      [true] /* 3rd-party non-tracker with permission */,
      [false, trackerBlocked] /* 3rd-party tracker */,
      [true] /* 3rd-party tracker with permission */,
      [true] /* same-site tracker */,
      [true] /* same-origin tracker */,
    ],
  },
];

(function() {
  settings.forEach(setting => {
    ok(true, JSON.stringify(setting));
    if (setting.setup) {
      add_task(async _ => {
        setting.setup();
      });
    }

    testCases.forEach(test => {
      let [hasStorageAccess, expectedBlockingNotifications] = test.cases[
        settings.indexOf(setting)
      ];
      let callback = hasStorageAccess
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
            true,
          ],
        ],
        expectedBlockingNotifications,
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
