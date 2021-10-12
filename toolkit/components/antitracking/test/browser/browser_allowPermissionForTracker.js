// This test works by setting up an exception for the tracker domain, which
// disables all the anti-tracking tests.

/* import-globals-from antitracking_head.js */

add_task(async _ => {
  PermissionTestUtils.add(
    "https://tracking.example.org",
    "cookie",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://tracking.example.com",
    "cookie",
    Services.perms.ALLOW_ACTION
  );
  // Grant interaction permission so we can directly call
  // requestStorageAccess from the tracker.
  PermissionTestUtils.add(
    "https://tracking.example.org",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  registerCleanupFunction(_ => {
    Services.perms.removeAll();
  });
});

AntiTracking._createTask({
  name: "Test that we do honour a cookie permission for nested windows",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: false,
  allowList: false,
  callback: async _ => {
    document.cookie = "name=value";
    ok(document.cookie != "", "Nothing is blocked");

    // requestStorageAccess should resolve
    let dwu = SpecialPowers.getDOMWindowUtils(window);
    let helper = dwu.setHandlingUserInput(true);
    await document
      .requestStorageAccess()
      .then(() => {
        ok(true, "Should grant storage access");
      })
      .catch(() => {
        ok(false, "Should grant storage access");
      });
    helper.destruct();
  },
  extraPrefs: null,
  expectedBlockingNotifications: 0,
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
