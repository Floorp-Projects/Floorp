add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_REJECT],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Disabling content blocking for this page");
  gProtectionsHandler.disableForCurrentPage();

  // The previous function reloads the browser, so wait for it to load again!
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, {}, async function(obj) {
    await new content.Promise(async resolve => {
      let document = content.document;
      let window = document.defaultView;

      is(document.cookie, "", "No cookies for me");

      await window
        .fetch("server.sjs")
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-not-present", "We should not have cookies");
        });

      document.cookie = "name=value";
      ok(document.cookie.includes("name=value"), "Some cookies for me");
      ok(document.cookie.includes("foopy=1"), "Some cookies for me");

      await window
        .fetch("server.sjs")
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-present", "We should have cookies");
        });

      ok(document.cookie.length, "Some Cookies for me");

      resolve();
    });
  });

  info("Enabling content blocking for this page");
  gProtectionsHandler.enableForCurrentPage();

  // The previous function reloads the browser, so wait for it to load again!
  await BrowserTestUtils.browserLoaded(browser);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
