add_task(async function() {
  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_REJECT],
      ["network.cookie.cookieBehavior.pbmode", BEHAVIOR_REJECT],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
      ["dom.ipc.processCount", 4],
    ],
  });

  info("First tab opened");
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "empty.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Disabling content blocking for this page");
  gProtectionsHandler.disableForCurrentPage();

  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(browser, [], async _ => {
    is(content.document.cookie, "", "No cookie set");
    content.document.cookie = "a=b";
    is(content.document.cookie, "a=b", "Cookie set");
  });

  info("Second tab opened");
  let tab2 = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "empty.html"
  );
  gBrowser.selectedTab = tab2;

  let browser2 = gBrowser.getBrowserForTab(tab2);
  await BrowserTestUtils.browserLoaded(browser2);

  await SpecialPowers.spawn(browser2, [], async _ => {
    is(content.document.cookie, "a=b", "Cookie set");
  });

  info("Removing tabs");
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);

  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
