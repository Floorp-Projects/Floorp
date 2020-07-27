add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  gProtectionsHandler.disableForCurrentPage();

  // The previous function reloads the browser, so wait for it to load again!
  await BrowserTestUtils.browserLoaded(browser);

  // Now load subresources from a few third-party origins.
  // We should expect to see none of these origins in the content blocking log at the end.
  await fetch(
    "https://test1.example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=image"
  )
    .then(r => r.text())
    .then(text => {
      is(text, "0", "Cookies received for images");
    });

  await fetch(
    "https://test2.example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=image"
  )
    .then(r => r.text())
    .then(text => {
      is(text, "0", "Cookies received for images");
    });

  info("Creating a 3rd party content");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE,
        blockingCallback: (async _ => {}).toString(),
        nonBlockingCallback: (async _ => {}).toString(),
      },
    ],
    async function(obj) {
      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = function() {
          info("Sending code to the 3rd party content");
          ifr.contentWindow.postMessage(obj.blockingCallback, "*");
        };

        content.addEventListener("message", function msg(event) {
          if (event.data.type == "finish") {
            content.removeEventListener("message", msg);
            resolve();
            return;
          }

          if (event.data.type == "ok") {
            ok(event.data.what, event.data.msg);
            return;
          }

          if (event.data.type == "info") {
            info(event.data.msg);
            return;
          }

          ok(false, "Unknown message");
        });

        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
      });
    }
  );

  let expectTrackerFound = item => {
    is(
      item[0],
      Ci.nsIWebProgressListener.STATE_LOADED_LEVEL_1_TRACKING_CONTENT,
      "Correct blocking type reported"
    );
    is(item[1], true, "Correct blocking status reported");
    ok(item[2] >= 1, "Correct repeat count reported");
  };

  let log = JSON.parse(await browser.getContentBlockingLog());
  for (let trackerOrigin in log) {
    is(
      trackerOrigin + "/",
      TEST_3RD_PARTY_DOMAIN,
      "Correct tracker origin must be reported"
    );
    let originLog = log[trackerOrigin];
    is(originLog.length, 1, "We should have 1 entry in the compressed log");
    expectTrackerFound(originLog[0]);
  }

  gProtectionsHandler.enableForCurrentPage();

  // The previous function reloads the browser, so wait for it to load again!
  await BrowserTestUtils.browserLoaded(browser);

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
