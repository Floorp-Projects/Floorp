/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", true],
      ["dom.storage_access.auto_grants.delayed", false],
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.prompt.testing", false],
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
      [
        "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
        "tracking.example.com,tracking.example.org",
      ],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Let's create an iframe and run the test there.
  let page = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "workerIframe.html";
  await ContentTask.spawn(browser, page, async function(page) {
    await new content.Promise(resolve => {
      let ifr = content.document.createElement("iframe");
      ifr.id = "test";

      content.addEventListener("message", e => {
        if (e.data.type == "finish") {
          resolve();
          return;
        }

        if (e.data.type == "info") {
          info(e.data.msg);
          return;
        }

        if (e.data.type == "ok") {
          ok(e.data.what, e.data.msg);
          return;
        }

        ok(false, "Unknown message");
      });

      content.document.body.appendChild(ifr);
      ifr.src = page;
    });
  });

  info("Removing the tab");
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
