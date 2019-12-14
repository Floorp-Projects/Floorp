/* eslint-disable prettier/prettier */

function runTest(obj) {
  add_task(async _ => {
    info("Test: " + obj.testName);
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        [
          "network.cookie.cookieBehavior",
          Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
        ],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        ["privacy.storagePrincipal.enabledForTrackers", false],
        [
          "privacy.trackingprotection.socialtracking.enabled",
          obj.protectionEnabled,
        ],
        ["privacy.socialtracking.block_cookies.enabled", obj.cookieBlocking],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      browser,
      [
        {
          page: TEST_3RD_PARTY_DOMAIN_STP + TEST_PATH + "localStorage.html",
          image: TEST_3RD_PARTY_DOMAIN_STP + TEST_PATH + "raptor.jpg",
          loading: obj.loading,
          result: obj.result,
        },
      ],
      async obj => {
        let loading = await new content.Promise(resolve => {
          let image = new content.Image();
          image.src = obj.image + "?" + Math.random();
          image.onload = _ => resolve(true);
          image.onerror = _ => resolve(false);
        });

        is(loading, obj.loading, "Loading expected");

        if (obj.loading) {
          let ifr = content.document.createElement("iframe");
          ifr.setAttribute("id", "ifr");
          ifr.setAttribute("src", obj.page);

          info("Iframe loading...");
          await new content.Promise(resolve => {
            ifr.onload = resolve;
            content.document.body.appendChild(ifr);
          });

          let p = new Promise(resolve => {
            content.addEventListener(
              "message",
              e => {
                resolve(e.data);
              },
              { once: true }
            );
          });

          info("Setting localStorage value...");
          ifr.contentWindow.postMessage("test", "*");

          info("Getting the value...");
          let value = await p;
          is(value.status, obj.result, "We expect to succeed");
        }
      }
    );

    info("Checking content blocking log.");
    let contentBlockingLog = JSON.parse(await browser.getContentBlockingLog());
    let origins = Object.keys(contentBlockingLog);
    is(origins.length, 1, "There should be one origin entry in the log.");
    for (let origin of origins) {
      is(
        origin + "/",
        TEST_3RD_PARTY_DOMAIN_STP,
        "Correct tracker origin must be reported"
      );
      Assert.deepEqual(
        contentBlockingLog[origin],
        obj.expectedLogItems,
        "Content blocking log should be as expected"
      );
    }

    BrowserTestUtils.removeTab(tab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });
}

runTest({
  testName:
    "Socialtracking-annotation feature enabled but not considered for tracking detection.",
  protectionEnabled: false,
  loading: true,
  cookieBlocking: false,
  result: true,
  expectedLogItems: [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED, true, 1],
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED_SOCIALTRACKER, true, 1],
  ],
});

runTest({
  testName:
    "Socialtracking-annotation feature enabled and considered for tracking detection.",
  protectionEnabled: false,
  loading: true,
  cookieBlocking: true,
  result: false,
  expectedLogItems: [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED, true, 1],
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED_SOCIALTRACKER, true, 1],
    [Ci.nsIWebProgressListener.STATE_LOADED_SOCIALTRACKING_CONTENT, true, 2],
    [Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_SOCIALTRACKER, true, 2],
  ],
});

runTest({
  testName: "Socialtracking-protection feature enabled.",
  protectionEnabled: true,
  loading: false,
  cookieBlocking: true,
  result: false,
  expectedLogItems: [
    [Ci.nsIWebProgressListener.STATE_BLOCKED_SOCIALTRACKING_CONTENT, true, 1],
  ],
});
