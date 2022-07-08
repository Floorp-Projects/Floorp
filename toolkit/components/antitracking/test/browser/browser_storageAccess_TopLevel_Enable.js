add_task(async function testDefaultDisabled() {
  let value = Services.prefs.getBoolPref(
    "dom.storage_access.forward_declared.enabled"
  );
  ok(!value, "dom.storage_access.forward_declared.enabled should be false");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async _ => {
    ok(
      content.window.requestStorageAccessUnderSite == undefined,
      "API should not be on the window"
    );
    ok(
      content.window.completeStorageAccessRequestFromSite == undefined,
      "API should not be on the window"
    );
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testExplicitlyDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.storage_access.forward_declared.enabled", false]],
  });
  let value = Services.prefs.getBoolPref(
    "dom.storage_access.forward_declared.enabled"
  );
  ok(!value, "dom.storage_access.forward_declared.enabled should be false");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async _ => {
    ok(
      content.window.requestStorageAccessUnderSite == undefined,
      "API should not be on the window"
    );
    ok(
      content.window.completeStorageAccessRequestFromSite == undefined,
      "API should not be on the window"
    );
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testExplicitlyEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
    ],
  });
  let value = Services.prefs.getBoolPref(
    "dom.storage_access.forward_declared.enabled"
  );
  ok(value, "dom.storage_access.forward_declared.enabled should be true");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async _ => {
    ok(
      content.document.requestStorageAccessUnderSite != undefined,
      "API should be on the window"
    );
    ok(
      content.document.completeStorageAccessRequestFromSite != undefined,
      "API should be on the window"
    );
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
