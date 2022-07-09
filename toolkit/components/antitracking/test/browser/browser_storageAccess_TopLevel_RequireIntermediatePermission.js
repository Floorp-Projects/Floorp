add_task(async function testIntermediatePermissionRequired() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      [
        "network.cookie.cookieBehavior",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(false, "Must not resolve.");
    } catch {
      ok(true, "Must reject because we don't have the initial request.");
    }
  });

  await SpecialPowers.pushPermissions([
    {
      type: "AllowStorageAccessRequest^https://example.org",
      allow: 1,
      context: TEST_TOP_PAGE,
    },
  ]);

  await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(
        true,
        "Must resolve now that we have the permission from the embedee."
      );
    } catch {
      ok(false, "Must not reject.");
    }
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async () => {
  Services.perms.removeAll();
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
