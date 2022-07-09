add_task(async function testIntermediatePreferenceReadSameSite() {
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
    url: TEST_DOMAIN_7,
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
      type: "AllowStorageAccessRequest^https://example.com",
      allow: 1,
      context: TEST_DOMAIN_7,
    },
  ]);

  await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(false, "Must not resolve.");
    } catch {
      ok(true, "Must reject because the permission is cross site.");
    }
  });

  await SpecialPowers.pushPermissions([
    {
      type: "AllowStorageAccessRequest^https://example.org",
      allow: 1,
      context: TEST_DOMAIN_7,
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

  await SpecialPowers.pushPermissions([
    {
      type: "AllowStorageAccessRequest^https://example.org",
      allow: 1,
      context: TEST_DOMAIN_8,
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

// Note: TEST_DOMAIN_7 and TEST_DOMAIN_8 are Same-Site
add_task(async function testIntermediatePreferenceWriteCrossOrigin() {
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
    url: TEST_3RD_PARTY_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_DOMAIN_8], async tp => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.requestStorageAccessUnderSite(tp);
    try {
      await p;
      ok(
        true,
        "Must resolve- no funny business here, we just want to set the intermediate pref"
      );
    } catch {
      ok(false, "Must not reject.");
    }
  });

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    TEST_DOMAIN_8
  );
  // Important to note that this is the site but not origin of TEST_3RD_PARTY_PAGE
  var permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "AllowStorageAccessRequest^https://example.org"
  );
  ok(permission == Services.perms.ALLOW_ACTION);

  // Test that checking the permission across site works
  principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    TEST_DOMAIN_7
  );
  // Important to note that this is the site but not origin of TEST_3RD_PARTY_PAGE
  permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "AllowStorageAccessRequest^https://example.org"
  );
  ok(permission == Services.perms.ALLOW_ACTION);

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
