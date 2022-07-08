add_task(async function testUserActivations() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_ACCEPT],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  // Part 1: open the embedded site as a top level
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_3RD_PARTY_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
    // Part 2: requestStorageAccessUnderSite without activation
    var p = content.document.requestStorageAccessUnderSite(tp);
    try {
      await p;
      ok(false, "Must not resolve without user activation.");
    } catch {
      ok(true, "Must reject without user activation.");
    }
    // Part 3: requestStorageAccessUnderSite with activation
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    p = content.document.requestStorageAccessUnderSite(tp);
    try {
      await p;
      ok(true, "Must resolve with user activation and autogrant.");
    } catch {
      ok(false, "Must not reject with user activation.");
    }
  });
  // Part 4: open the embedding site as a top level
  let tab2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser2 = tab2.linkedBrowser;
  await SpecialPowers.spawn(browser2, [TEST_3RD_PARTY_DOMAIN], async tp => {
    // Part 5: completeStorageAccessRequestFromSite without activation
    var p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(true, "Must resolve without user activation.");
    } catch {
      ok(false, "Must not reject without user activation in this context.");
    }
  });
  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(tab2);
});

add_task(async () => {
  Services.perms.removeAll();
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
