/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const pageEmptyURL =
  "https://example.com/browser/remote/cdp/test/browser/page/doc_empty.html";

/*
 * Set the optional preference to disallow access to localhost when offline. This is
 * required because `example.com` resolves to `localhost` in the tests and therefore
 * would still be accessible even though we are simulating being offline.
 * By setting this preference, we make sure that these connections to `localhost`
 * (and by extension, to `example.com`) will fail when we are offline.
 */
Services.prefs.setBoolPref("network.disable-localhost-when-offline", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.disable-localhost-when-offline");
});

/**
 * Acts just as `add_task`, but does cleanup afterwards
 *
 * @param {Function} taskFn
 */
function add_networking_task(taskFn) {
  add_task(async client => {
    try {
      await taskFn(client);
    } finally {
      Services.io.offline = false;
    }
  });
}

add_networking_task(async function offlineWithoutArguments({ client }) {
  const { Network } = client;

  await Assert.rejects(
    Network.emulateNetworkConditions(),
    /offline: boolean value expected/,
    "Fails without any arguments"
  );
});

add_networking_task(async function offlineWithEmptyArguments({ client }) {
  const { Network } = client;

  await Assert.rejects(
    Network.emulateNetworkConditions({}),
    /offline: boolean value expected/,
    "Fails with only empty arguments"
  );
});

add_networking_task(async function offlineWithInvalidArguments({ client }) {
  const { Network } = client;
  const testTable = [null, undefined, 1, "foo", [], {}];

  for (const testCase of testTable) {
    const testType = typeof testCase;
    await Assert.rejects(
      Network.emulateNetworkConditions({ offline: testCase }),
      /offline: boolean value expected/,
      `Fails with ${testType}-type argument for offline`
    );
  }
});

add_networking_task(async function offlineWithUnsupportedArguments({ client }) {
  const { Network } = client;

  // Random valid values for the Network.emulateNetworkConditions command, even though we don't support them yet
  const args = {
    offline: true,
    latency: 500,
    downloadThroughput: 500,
    uploadThroughput: 500,
    connectionType: "cellular2g",
    someFutureArg: false,
  };

  await Network.emulateNetworkConditions(args);

  ok(true, "No errors should be thrown due to non-implemented arguments");
});

add_networking_task(async function emulateOfflineWhileOnline({ client }) {
  const { Network } = client;

  // Assert we're online to begin with
  await assertOfflineStatus(false);

  // Assert for offline
  await Network.emulateNetworkConditions({ offline: true });
  await assertOfflineStatus(true);

  // Assert we really can't navigate after setting offline
  await assertOfflineNavigationFails();
});

add_networking_task(async function emulateOfflineWhileOffline({ client }) {
  const { Network } = client;

  // Assert we're online to begin with
  await assertOfflineStatus(false);

  // Assert for offline
  await Network.emulateNetworkConditions({ offline: true });
  await assertOfflineStatus(true);

  // Assert for no-offline event, because we're offline - and changing to offline - so nothing changes
  await Network.emulateNetworkConditions({ offline: true });
  await assertOfflineStatus(true);

  // Assert we still can't navigate after setting offline twice
  await assertOfflineNavigationFails();
});

add_networking_task(async function emulateOnlineWhileOnline({ client }) {
  const { Network } = client;

  // Assert we're online to begin with
  await assertOfflineStatus(false);

  // Assert for no-offline event, because we're online - and changing to online - so nothing changes
  await Network.emulateNetworkConditions({ offline: false });
  await assertOfflineStatus(false);
});

add_networking_task(async function emulateOnlineWhileOffline({ client }) {
  const { Network } = client;

  // Assert we're online to begin with
  await assertOfflineStatus(false);

  // Assert for offline event, because we're online - and changing to offline
  const offlineChanged = Promise.race([
    BrowserTestUtils.waitForContentEvent(
      gBrowser.selectedBrowser,
      "online",
      true
    ),
    BrowserTestUtils.waitForContentEvent(
      gBrowser.selectedBrowser,
      "offline",
      true
    ),
  ]);

  await Network.emulateNetworkConditions({ offline: true });

  info("Waiting for offline event on window");
  is(await offlineChanged, "offline", "Only the offline-event should fire");
  await assertOfflineStatus(true);

  // Assert for online event, because we're offline - and changing to online
  const offlineChangedBack = Promise.race([
    BrowserTestUtils.waitForContentEvent(
      gBrowser.selectedBrowser,
      "online",
      true
    ),
    BrowserTestUtils.waitForContentEvent(
      gBrowser.selectedBrowser,
      "offline",
      true
    ),
  ]);
  await Network.emulateNetworkConditions({ offline: false });

  info("Waiting for online event on window");
  is(await offlineChangedBack, "online", "Only the online-event should fire");
  await assertOfflineStatus(false);
});

/**
 * Navigates to a page, and asserting any status code to appear
 */
async function assertOfflineNavigationFails() {
  const browser = gBrowser.selectedTab.linkedBrowser;
  let netErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);

  BrowserTestUtils.startLoadingURIString(browser, pageEmptyURL);
  await netErrorLoaded;
}

/**
 * Checks on the page what the value of window.navigator.onLine is on the currently navigated page
 *
 * @param {boolean} offline
 *    True if offline is expected
 */
function assertOfflineStatus(offline) {
  is(
    Services.io.offline,
    offline,
    "Services.io.offline should be " + (offline ? "true" : "false")
  );

  return SpecialPowers.spawn(gBrowser.selectedBrowser, [offline], offline => {
    is(
      content.navigator.onLine,
      !offline,
      "Page should be " + (offline ? "offline" : "online")
    );
  });
}
