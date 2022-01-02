/* eslint-disable mozilla/no-arbitrary-setTimeout */
/**
 * This test ensures that http-on-modify-request is dispatched for channels that
 * are blocked by tracking protection.  It sets up a page with a third-party script
 * resource on it that is blocked by TP, and sets up an http-on-modify-request
 * observer which waits to be notified about that resource.  The test would time out
 * if the http-on-modify-request notification isn't dispatched before the channel is
 * canceled.
 */

let gExpectedResourcesSeen = 0;
async function onModifyRequest() {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observer(subject, topic, data) {
      let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);
      let spec = httpChannel.URI.spec;
      info("Observed channel for " + spec);
      if (httpChannel.URI.prePath + "/" != TEST_3RD_PARTY_DOMAIN_TP) {
        return;
      }
      if (spec.endsWith("empty.js")) {
        ok(true, "Correct resource observed");
        ++gExpectedResourcesSeen;
      } else if (spec.endsWith("empty.js?redirect")) {
        httpChannel.redirectTo(
          Services.io.newURI(spec.replace("empty.js?redirect", "head.js"))
        );
      } else if (spec.endsWith("empty.js?redirect2")) {
        httpChannel.suspend();
        setTimeout(() => {
          httpChannel.redirectTo(
            Services.io.newURI(spec.replace("empty.js?redirect2", "head.js"))
          );
          httpChannel.resume();
        }, 100);
      } else if (spec.endsWith("head.js")) {
        ++gExpectedResourcesSeen;
      }
      if (gExpectedResourcesSeen == 3) {
        Services.obs.removeObserver(observer, "http-on-modify-request");
        resolve();
      }
    }, "http-on-modify-request");
  });
}

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
      ["privacy.trackingprotection.testing.report_blocked_node", true],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  let promise = onModifyRequest();

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_EMBEDDER_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await promise;

  info("Verify the number of tracking nodes found");
  await SpecialPowers.spawn(
    browser,
    [{ expected: gExpectedResourcesSeen }],
    async function(obj) {
      is(
        content.document.blockedNodeByClassifierCount,
        obj.expected,
        "Expected tracking nodes found"
      );
    }
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
});
