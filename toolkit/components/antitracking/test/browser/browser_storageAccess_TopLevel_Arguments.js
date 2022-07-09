add_task(async function testArgumentInRequestStorageAccessUnderSite() {
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
    url: TEST_4TH_PARTY_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async _ => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.requestStorageAccessUnderSite("blob://test");
    try {
      await p;
      ok(false, "Blob URLs must be rejected.");
    } catch {
      ok(true, "Must reject.");
    }

    p = content.document.requestStorageAccessUnderSite("about:config");
    try {
      await p;
      ok(false, "about URLs must be rejected.");
    } catch {
      ok(true, "Must reject.");
    }

    p = content.document.requestStorageAccessUnderSite("qwertyuiop");
    try {
      await p;
      ok(false, "Non URLs must be rejected.");
    } catch {
      ok(true, "Must reject.");
    }

    p = content.document.requestStorageAccessUnderSite("");
    try {
      await p;
      ok(false, "Nullstring must be rejected.");
    } catch {
      ok(true, "Must reject.");
    }
  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testArgumentInCompleteStorageAccessRequest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_ACCEPT],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async _ => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.completeStorageAccessRequestFromSite(
      "blob://test"
    );
    try {
      await p;
      ok(false, "Blob URLs must be rejected.");
    } catch {
      ok(true, "Must reject.");
    }

    p = content.document.completeStorageAccessRequestFromSite("about:config");
    try {
      await p;
      ok(false, "about URLs must be rejected.");
    } catch {
      ok(true, "Must reject.");
    }

    p = content.document.completeStorageAccessRequestFromSite("qwertyuiop");
    try {
      await p;
      ok(false, "Non URLs must be rejected.");
    } catch {
      ok(true, "Must reject.");
    }

    p = content.document.completeStorageAccessRequestFromSite("");
    try {
      await p;
      ok(false, "Nullstring must be rejected.");
    } catch {
      ok(true, "Must reject.");
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
