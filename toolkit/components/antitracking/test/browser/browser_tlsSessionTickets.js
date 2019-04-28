function isIsolated(key) {
  return key.charAt(7) == "i";
}

function hasTopWindowOrigin(key, origin) {
  let tokenAtEnd = `{{${origin}}}`;
  let endPart = key.slice(-tokenAtEnd.length);
  return endPart == tokenAtEnd;
}

function hasAnyTopWindowOrigin(key) {
  return !!key.match(/{{[^}]+}}/);
}

add_task(async function() {
  info("Starting tlsSessionTickets test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.cache.disk.enable", false],
    ["browser.cache.memory.enable", false],
    ["browser.contentblocking.allowlist.annotations.enabled", true],
    ["browser.contentblocking.allowlist.storage.enabled", true],
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

  const trackingURL = "https://tlsresumptiontest.example.org/browser/toolkit/components/antitracking/test/browser/empty.js";
  const kTopic = "http-on-examine-response";

  let resumedState = [];
  let hashKeys = [];

  function observer(subject, topic, data) {
    if (topic != kTopic) {
      return;
    }

    subject.QueryInterface(Ci.nsIChannel);
    if (subject.URI.spec != trackingURL) {
      return;
    }

    resumedState.push(subject.securityInfo
                             .QueryInterface(Ci.nsISSLSocketControl)
                             .resumed);
    hashKeys.push(subject.QueryInterface(Ci.nsIHttpChannelInternal)
                         .connectionInfoHashKey);
  }

  Services.obs.addObserver(observer, kTopic);
  registerCleanupFunction(() => Services.obs.removeObserver(observer, kTopic));

  info("Loading tracking scripts and tracking images");
  await ContentTask.spawn(browser, {trackingURL}, async function(obj) {
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => { src.onload = resolve; });
      content.document.body.appendChild(src);
      src.src = obj.trackingURL;
      await p;
    }
  });

  // Load our tracking URL two more times, but this time in the first-party context.
  // The TLS session should be resumed the second time here.
  await fetch(trackingURL);
  // Wait a little bit before issuing the second load to ensure both don't happen
  // at the same time.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 10));
  await fetch(trackingURL).then(() => {
    is(resumedState.length, 3, "We should have observed 3 loads for " + trackingURL);
    ok(!resumedState[0], "The first load should NOT have been resumed");
    ok(!resumedState[1], "The second load should NOT have been resumed");
    ok(resumedState[2], "The third load SHOULD have been resumed");

    // We also verify that the hashKey of the first connection is different to
    // both the second and third connections, and that the hashKey of the
    // second and third connections are the same.  The reason why this check is
    // done is that the private bit on the connection info object is used to
    // construct the hash key, so when the connection is isolated because it
    // comes from a third-party tracker context, its hash key must be
    // different.
    is(hashKeys.length, 3, "We should have observed 3 loads for " + trackingURL);
    is(hashKeys[1], hashKeys[2], "The second and third hash keys should match");
    isnot(hashKeys[0], hashKeys[1], "The first and second hash keys should not match");

    ok(isIsolated(hashKeys[0]), "The first connection must have been isolated");
    ok(!isIsolated(hashKeys[1]), "The second connection must not have been isolated");
    ok(!isIsolated(hashKeys[2]), "The third connection must not have been isolated");
    ok(hasTopWindowOrigin(hashKeys[0], TEST_DOMAIN.replace(/\/$/, "")),
       "The first connection must be bound to its top-level window");
    ok(!hasAnyTopWindowOrigin(hashKeys[1]),
       "The second connection must not be bound to a top-level window");
    ok(!hasAnyTopWindowOrigin(hashKeys[2]),
       "The third connection must not be bound to a top-level window");
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
});

