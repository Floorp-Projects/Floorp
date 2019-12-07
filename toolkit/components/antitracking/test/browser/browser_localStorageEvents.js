add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
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
});

add_task(async function testLocalStorageEventPropagation() {
  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_DOMAIN + TEST_PATH + "localStorage.html",
      },
    ],
    async obj => {
      info("Creating tracker iframe");

      let ifr = content.document.createElement("iframe");
      ifr.src = obj.page;

      await new content.Promise(resolve => {
        ifr.onload = function() {
          resolve();
        };
        content.document.body.appendChild(ifr);
      });

      info("LocalStorage should be blocked.");
      await new content.Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            if (e.data.type == "test") {
              is(e.data.status, false, "LocalStorage blocked");
            } else {
              ok(false, "Unknown message");
            }
            resolve();
          },
          { once: true }
        );
        ifr.contentWindow.postMessage("test", "*");
      });

      info("Let's open the popup");
      await new content.Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            if (e.data.type == "test") {
              is(e.data.status, true, "LocalStorage unblocked");
            } else {
              ok(false, "Unknown message");
            }
            resolve();
          },
          { once: true }
        );
        ifr.contentWindow.postMessage("open", "*");
      });
    }
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function testBlockedLocalStorageEventPropagation() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
        "tracking.example.com,tracking.example.org",
      ],
    ],
  });

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_DOMAIN + TEST_PATH + "localStorage.html",
      },
    ],
    async obj => {
      info("Creating tracker iframe");

      let ifr = content.document.createElement("iframe");
      ifr.src = obj.page;

      await new content.Promise(resolve => {
        ifr.onload = function() {
          resolve();
        };
        content.document.body.appendChild(ifr);
      });

      info("LocalStorage should be blocked.");
      await new content.Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            if (e.data.type == "test") {
              is(e.data.status, false, "LocalStorage blocked");
            } else {
              ok(false, "Unknown message");
            }
            resolve();
          },
          { once: true }
        );
        ifr.contentWindow.postMessage("test", "*");
      });

      info("Let's open the popup");
      await new content.Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            if (e.data.type == "test") {
              is(e.data.status, false, "LocalStorage still blocked");
            } else {
              ok(false, "Unknown message");
            }
            resolve();
          },
          { once: true }
        );
        ifr.contentWindow.postMessage("open and test", "*");
      });
    }
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();

  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
