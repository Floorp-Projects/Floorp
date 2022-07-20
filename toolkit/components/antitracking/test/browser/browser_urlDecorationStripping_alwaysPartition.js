// This test ensures that the URL decoration annotations service works as
// expected, and also we successfully downgrade document.referrer to the
// eTLD+1 URL when tracking identifiers controlled by this service are
// present in the referrer URI.

/* import-globals-from antitracking_head.js */

"use strict";

const trackerBlocked = Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER;

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const COLLECTION_NAME = "anti-tracking-url-decoration";
const PREF_NAME = "privacy.restrict3rdpartystorage.url_decorations";
const TOKEN_1 = "fooBar";
const TOKEN_2 = "foobaz";
const TOKEN_3 = "fooqux";
const TOKEN_4 = "bazqux";

const APS_PREF =
  "privacy.partition.always_partition_third_party_non_cookie_storage";

const token_1 = TOKEN_1.toLowerCase();

const DOMAIN = TEST_DOMAIN_3;
const SUB_DOMAIN = "https://sub1.xn--hxajbheg2az3al.xn--jxalpdlp/";
const TOP_PAGE_WITHOUT_TRACKING_IDENTIFIER =
  SUB_DOMAIN + TEST_PATH + "page.html";
const TOP_PAGE_WITH_TRACKING_IDENTIFIER =
  TOP_PAGE_WITHOUT_TRACKING_IDENTIFIER + "?" + TOKEN_1 + "=123";

add_task(async _ => {
  let uds = Cc["@mozilla.org/tracking-url-decoration-service;1"].getService(
    Ci.nsIURLDecorationAnnotationsService
  );

  let records = [
    {
      id: "1",
      last_modified: 1000000000000001,
      schema: Date.now(),
      token: TOKEN_1,
    },
  ];

  // Add some initial data
  async function emitSync() {
    await RemoteSettings(COLLECTION_NAME).emit("sync", {
      data: { current: records },
    });
  }
  let db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), [records[0]]);
  await emitSync();

  await uds.ensureUpdated();

  let list = Preferences.get(PREF_NAME).split(" ");
  ok(list.includes(TOKEN_1), "Token must now be available in " + PREF_NAME);
  ok(Preferences.locked(PREF_NAME), PREF_NAME + " must be locked");

  async function verifyList(array, not_array) {
    await emitSync();

    await uds.ensureUpdated();

    list = Preferences.get(PREF_NAME).split(" ");
    for (let token of array) {
      ok(
        list.includes(token),
        token + " must now be available in " + PREF_NAME
      );
    }
    for (let token of not_array) {
      ok(
        !list.includes(token),
        token + " must not be available in " + PREF_NAME
      );
    }
    ok(Preferences.locked(PREF_NAME), PREF_NAME + " must be locked");
  }

  records.push(
    {
      id: "2",
      last_modified: 1000000000000002,
      schema: Date.now(),
      token: TOKEN_2,
    },
    {
      id: "3",
      last_modified: 1000000000000003,
      schema: Date.now(),
      token: TOKEN_3,
    },
    {
      id: "4",
      last_modified: 1000000000000005,
      schema: Date.now(),
      token: TOKEN_4,
    }
  );

  await verifyList([TOKEN_1, TOKEN_2, TOKEN_3, TOKEN_4], []);

  records.pop();

  await verifyList([TOKEN_1, TOKEN_2, TOKEN_3], [TOKEN_4]);

  is(
    Services.eTLD.getBaseDomain(Services.io.newURI(DOMAIN)),
    Services.eTLD.getBaseDomain(Services.io.newURI(SUB_DOMAIN)),
    "Sanity check"
  );

  registerCleanupFunction(async _ => {
    records = [];
    await db.clear();
    await emitSync();
  });
});

AntiTracking._createTask({
  name:
    "Test that we do not downgrade document.referrer when it does not contain a tracking identifier",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: true,
  allowList: false,
  callback: async _ => {
    let ref = new URL(document.referrer);
    is(
      ref.hostname,
      "sub1.xn--hxajbheg2az3al.xn--jxalpdlp",
      "Hostname shouldn't be stripped"
    );
    ok(ref.pathname.length > 1, "Path must not be trimmed");
    // eslint-disable-next-line no-unused-vars
    for (let entry of ref.searchParams.entries()) {
      ok(false, "No query parameters should be found");
    }
  },
  extraPrefs: [
    ["network.http.referer.defaultPolicy", 3], // Ensure we don't downgrade because of the default policy.
    ["network.http.referer.defaultPolicy.trackers", 3],
    [APS_PREF, true],
  ],
  expectedBlockingNotifications: trackerBlocked,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  topPage: TOP_PAGE_WITHOUT_TRACKING_IDENTIFIER,
});

AntiTracking._createTask({
  name:
    "Test that we do not downgrade document.referrer when it does not contain a tracking identifier even though it gets downgraded to origin only due to the default referrer policy",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: true,
  allowList: false,
  callback: async _ => {
    let ref = new URL(document.referrer);
    is(
      ref.hostname,
      "sub1.xn--hxajbheg2az3al.xn--jxalpdlp",
      "Hostname shouldn't be stripped"
    );
    is(ref.pathname.length, 1, "Path must be trimmed");
    // eslint-disable-next-line no-unused-vars
    for (let entry of ref.searchParams.entries()) {
      ok(false, "No query parameters should be found");
    }
  },
  extraPrefs: [
    ["network.http.referer.defaultPolicy.trackers", 2],
    [APS_PREF, true],
  ],
  expectedBlockingNotifications: trackerBlocked,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  topPage: TOP_PAGE_WITHOUT_TRACKING_IDENTIFIER,
});

AntiTracking._createTask({
  name:
    "Test that we downgrade document.referrer when it contains a tracking identifier",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: true,
  allowList: false,
  callback: async _ => {
    let ref = new URL(document.referrer);
    is(
      ref.hostname,
      "xn--hxajbheg2az3al.xn--jxalpdlp",
      "Hostname should be stripped"
    );
    is(ref.pathname.length, 1, "Path must be trimmed");
    // eslint-disable-next-line no-unused-vars
    for (let entry of ref.searchParams.entries()) {
      ok(false, "No query parameters should be found");
    }
  },
  extraPrefs: [
    ["network.http.referer.defaultPolicy", 3], // Ensure we don't downgrade because of the default policy.
    ["network.http.referer.defaultPolicy.trackers", 3],
    [APS_PREF, true],
  ],
  expectedBlockingNotifications: trackerBlocked,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  topPage: TOP_PAGE_WITH_TRACKING_IDENTIFIER,
});

AntiTracking._createTask({
  name:
    "Test that we don't downgrade document.referrer when it contains a tracking identifier if it gets downgraded to origin only due to the default referrer policy because the tracking identifier wouldn't be present in the referrer any more",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: true,
  allowList: false,
  callback: async _ => {
    let ref = new URL(document.referrer);
    is(
      ref.hostname,
      "sub1.xn--hxajbheg2az3al.xn--jxalpdlp",
      "Hostname shouldn't be stripped"
    );
    is(ref.pathname.length, 1, "Path must be trimmed");
    // eslint-disable-next-line no-unused-vars
    for (let entry of ref.searchParams.entries()) {
      ok(false, "No query parameters should be found");
    }
  },
  extraPrefs: [
    ["network.http.referer.defaultPolicy.trackers", 2],
    [APS_PREF, true],
  ],
  expectedBlockingNotifications: trackerBlocked,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  topPage: TOP_PAGE_WITH_TRACKING_IDENTIFIER,
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
