/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userInteraction.document.interval", 1],
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

  let uri = Services.io.newURI(TEST_DOMAIN);
  is(
    PermissionTestUtils.testPermission(uri, "storageAccessAPI"),
    Services.perms.UNKNOWN_ACTION,
    "Before user-interaction we don't have a permission"
  );

  let promise = TestUtils.topicObserved("perm-changed", (aSubject, aData) => {
    let permission = aSubject.QueryInterface(Ci.nsIPermission);
    return (
      permission.type == "storageAccessAPI" &&
      permission.principal.URI.equals(uri)
    );
  });

  info("Simulating user-interaction.");
  await ContentTask.spawn(browser, null, async function() {
    content.document.userInteractionForTesting();
  });

  info("Waiting to have a permissions set.");
  await promise;

  // Let's see if the document is able to update the permission correctly.
  for (var i = 0; i < 3; ++i) {
    // Another perm-changed event should be triggered by the timer.
    promise = TestUtils.topicObserved("perm-changed", (aSubject, aData) => {
      let permission = aSubject.QueryInterface(Ci.nsIPermission);
      return (
        permission.type == "storageAccessAPI" &&
        permission.principal.URI.equals(uri)
      );
    });

    info("Simulating another user-interaction.");
    await ContentTask.spawn(browser, null, async function() {
      content.document.userInteractionForTesting();
    });

    info("Waiting to have a permissions set.");
    await promise;
  }

  // Let's disable the document.interval.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userInteraction.document.interval", 0]],
  });

  promise = new Promise(resolve => {
    let id;

    function observer(subject, topic, data) {
      ok(false, "Notification received!");
      Services.obs.removeObserver(observer, "perm-changed");
      clearTimeout(id);
      resolve();
    }

    Services.obs.addObserver(observer, "perm-changed");

    id = setTimeout(() => {
      ok(true, "No notification received!");
      Services.obs.removeObserver(observer, "perm-changed");
      resolve();
    }, 2000);
  });

  info("Simulating another user-interaction.");
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
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
