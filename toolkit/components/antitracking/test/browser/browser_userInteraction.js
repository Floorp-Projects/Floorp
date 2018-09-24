ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.contentblocking.enabled", true],
    ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
    ["privacy.trackingprotection.enabled", false],
    ["privacy.trackingprotection.pbmode.enabled", false],
    ["privacy.trackingprotection.annotate_channels", true],
  ]});

  await UrlClassifierTestUtils.addTestTrackers();

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let uri = Services.io.newURI(TEST_DOMAIN);
  is (Services.perms.testPermission(uri, "storageAccessAPI"), Services.perms.UNKNOWN_ACTION,
      "Before user-interaction we don't have a permission");

  let promise = TestUtils.topicObserved("perm-changed", (aSubject, aData) => {
    let permission = aSubject.QueryInterface(Ci.nsIPermission);
    return permission.type == "storageAccessAPI" &&
           permission.principal.URI.equals(uri);
  });

  info("Simulating user-interaction.");
  await ContentTask.spawn(browser, null, async function() {
    content.document.userInteractionForTesting();
  });

  info("Waiting to have a permissions set.");
  await promise;

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
});
