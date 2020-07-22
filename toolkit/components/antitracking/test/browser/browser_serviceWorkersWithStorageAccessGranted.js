/** This tests that the service worker can be used if we have storage access
 *  permission. We manually write the storage access permission into the
 *  permission manager to simulate the storage access has been granted. We would
 *  test the service worker three times. The fist time is to check the service
 *  work is allowed. The second time is to load again and check it won't hit
 *  assertion, this assertion would only be hit if we have registered a service
 *  worker, see Bug 1631234.
 *
 *  The third time is to load again but in a sandbox iframe to check it won't
 *  hit the assertion. See Bug 1637226 for details.
 *
 *  The fourth time is to load again in a nested iframe to check it won't hit
 *  the assertion. See Bug 1641153 for details.
 *  */

/* import-globals-from antitracking_head.js */

add_task(async _ => {
  // Manually add the storage permission.
  PermissionTestUtils.add(
    TEST_DOMAIN,
    "3rdPartyStorage^https://tracking.example.org",
    Services.perms.ALLOW_ACTION
  );

  registerCleanupFunction(_ => {
    Services.perms.removeAll();
  });

  AntiTracking._createTask({
    name:
      "Test that we can use service worker if we have the storage access permission",
    cookieBehavior: BEHAVIOR_REJECT_TRACKER,
    allowList: false,
    callback: async _ => {
      await navigator.serviceWorker
        .register("empty.js")
        .then(
          _ => {
            ok(true, "ServiceWorker can be used!");
          },
          _ => {
            ok(false, "ServiceWorker can be used!");
          }
        )
        .catch(e => ok(false, "Promise rejected: " + e));
    },
    extraPrefs: [
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
    expectedBlockingNotifications: 0,
    runInPrivateWindow: false,
    iframeSandbox: null,
    accessRemoval: null,
    callbackAfterRemoval: null,
  });

  AntiTracking._createTask({
    name:
      "Test again to check if we can still use service worker without hit the assertion.",
    cookieBehavior: BEHAVIOR_REJECT_TRACKER,
    allowList: false,
    callback: async _ => {
      await navigator.serviceWorker
        .register("empty.js")
        .then(
          _ => {
            ok(true, "ServiceWorker can be used!");
          },
          _ => {
            ok(false, "ServiceWorker can be used!");
          }
        )
        .catch(e => ok(false, "Promise rejected: " + e));
    },
    extraPrefs: [
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
    expectedBlockingNotifications: 0,
    runInPrivateWindow: false,
    iframeSandbox: null,
    accessRemoval: null,
    callbackAfterRemoval: null,
  });

  AntiTracking._createTask({
    name:
      "Test again to check if we cannot use service worker in a sandbox iframe without hit the assertion.",
    cookieBehavior: BEHAVIOR_REJECT_TRACKER,
    allowList: false,
    callback: async _ => {
      await navigator.serviceWorker
        .register("empty.js")
        .then(
          _ => {
            ok(false, "ServiceWorker cannot be used in sandbox iframe!");
          },
          _ => {
            ok(true, "ServiceWorker cannot be used in sandbox iframe!");
          }
        )
        .catch(e => ok(false, "Promise rejected: " + e));
    },
    extraPrefs: [
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
    expectedBlockingNotifications:
      Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications,
    runInPrivateWindow: false,
    iframeSandbox: "allow-scripts allow-same-origin",
    accessRemoval: null,
    callbackAfterRemoval: null,
  });

  const NESTED_THIRD_PARTY_PAGE =
    TEST_DOMAIN + TEST_PATH + "3rdPartyRelay.html?" + TEST_3RD_PARTY_PAGE;

  AntiTracking._createTask({
    name:
      "Test again to check if we cannot use service worker in a nested iframe without hit the assertion.",
    cookieBehavior: BEHAVIOR_REJECT_TRACKER,
    allowList: false,
    callback: async _ => {
      await navigator.serviceWorker
        .register("empty.js")
        .then(
          _ => {
            ok(false, "ServiceWorker cannot be used in nested iframe!");
          },
          _ => {
            ok(true, "ServiceWorker cannot be used in nested iframe!");
          }
        )
        .catch(e => ok(false, "Promise rejected: " + e));
    },
    extraPrefs: [
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
    expectedBlockingNotifications:
      Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
    // Due to the first-level iframe won't be considered as a third-party
    // tracking iframe in this test since it is loaded with the origin of the
    // top level. So, the test framework will reset the
    // `expectedBlockingNotifications` to 0. However, we actually expect to see
    // blocking notifications from the nested iframe where we do the test. So,
    // we have to skip resetting the blocking notifications for this test.
    dontResetExpectedBlockingNotifications: true,
    runInPrivateWindow: false,
    iframeSandbox: null,
    accessRemoval: null,
    callbackAfterRemoval: null,
    thirdPartyPage: NESTED_THIRD_PARTY_PAGE,
  });
});
