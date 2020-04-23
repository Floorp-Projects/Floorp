/** This tests that the service worker can be used if we have storage access
 *  permission. We manually write the storage access permission into the
 *  permission manager to simulate the storage access has been granted. We would
 *  test the service worker twice. The fist time is to check the service
 *  work is allowed. The second time is to load again and check it won't hit
 *  assertion, this assertion would only be hit if we have registered a service
 *  worker, see Bug 1631234. */

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
});
