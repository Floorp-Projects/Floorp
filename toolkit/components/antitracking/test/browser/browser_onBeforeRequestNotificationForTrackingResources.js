/**
 * This test ensures that onBeforeRequest is dispatched for webRequest loads that
 * are blocked by tracking protection.  It sets up a page with a third-party script
 * resource on it that is blocked by TP, and sets up an onBeforeRequest listener
 * which waits to be notified about that resource.  The test would time out if the
 * onBeforeRequest listener isn't called dispatched before the load is canceled.
 */

let extension;
add_task(async function() {
  extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["webRequest", "webRequestBlocking", "*://*/*"] },
    async background() {
      let gExpectedResourcesSeen = 0;
      function onBeforeRequest(details) {
        let spec = details.url;
        browser.test.log("Observed channel for " + spec);
        // We would use TEST_3RD_PARTY_DOMAIN_TP here, but the variable is inaccessible
        // since it is defined in head.js!
        if (!spec.startsWith("https://tracking.example.com/")) {
          return undefined;
        }
        if (spec.endsWith("empty.js")) {
          browser.test.succeed("Correct resource observed");
          ++gExpectedResourcesSeen;
        } else if (spec.endsWith("empty.js?redirect")) {
          return { redirectUrl: spec.replace("empty.js?redirect", "head.js") };
        } else if (spec.endsWith("head.js")) {
          ++gExpectedResourcesSeen;
        }
        if (gExpectedResourcesSeen == 2) {
          browser.webRequest.onBeforeRequest.removeListener(onBeforeRequest);
          browser.test.sendMessage("finish");
        }
        return undefined;
      }

      browser.webRequest.onBeforeRequest.addListener(
        onBeforeRequest,
        { urls: ["*://*/*"] },
        ["blocking"]
      );
      browser.test.sendMessage("ready");
    },
  });
  await extension.startup();
  await extension.awaitMessage("ready");
});

add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.enabled", true],
      // the test doesn't open a private window, so we don't care about this pref's value
      ["privacy.trackingprotection.pbmode.enabled", false],
      // tracking annotations aren't needed in this test, only TP is needed
      ["privacy.trackingprotection.annotate_channels", false],
      [
        "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
        "tracking.example.com,tracking.example.org",
      ],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  let promise = extension.awaitMessage("finish");

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_EMBEDDER_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await promise;

  info("Verify the number of tracking nodes found");
  await ContentTask.spawn(browser, { expected: 3 }, async function(obj) {
    is(
      content.document.blockedNodeByClassifierCount,
      obj.expected,
      "Expected tracking nodes found"
    );
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
  await extension.unload();
});
