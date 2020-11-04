/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const pageEmptyURL =
  "http://example.com/browser/remote/test/browser/page/doc_empty.html";

/**
 * Acts just as `add_task`, but does cleanup afterwards
 * @param taskFn
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

  let errorThrown = "";
  try {
    await Network.emulateNetworkConditions();
  } catch (e) {
    errorThrown = e.message;
  }

  ok(
    errorThrown.match(/offline: boolean value expected/),
    "Fails without any arguments"
  );
});

add_networking_task(async function offlineWithEmptyArguments({ client }) {
  const { Network } = client;

  let errorThrown = "";
  try {
    await Network.emulateNetworkConditions({});
  } catch (e) {
    errorThrown = e.message;
  }

  ok(
    errorThrown.match(/offline: boolean value expected/),
    "Fails with only empty arguments"
  );
});

add_networking_task(async function offlineWithInvalidArguments({ client }) {
  const { Network } = client;
  const testTable = [null, undefined, 1, "foo", [], {}];

  for (const testCase of testTable) {
    let errorThrown = "";
    try {
      await Network.emulateNetworkConditions({ offline: testCase });
    } catch (e) {
      errorThrown = e.message;
    }

    const testType = typeof testCase;

    ok(
      errorThrown.match(/offline: boolean value expected/),
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
  // Tests always connect to localhost, and per bug 87717, localhost is now
  // reachable in offline mode.  To avoid this, disable any proxy.
  const proxyPrefValue = SpecialPowers.getIntPref("network.proxy.type");
  const diskPrefValue = SpecialPowers.getBoolPref("browser.cache.disk.enable");
  const memoryPrefValue = SpecialPowers.getBoolPref(
    "browser.cache.memory.enable"
  );
  SpecialPowers.pushPrefEnv({
    set: [
      ["network.proxy.type", 0],
      ["browser.cache.disk.enable", false],
      ["browser.cache.memory.enable", false],
    ],
  });

  try {
    const browser = gBrowser.selectedTab.linkedBrowser;
    let netErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);

    await BrowserTestUtils.loadURI(browser, pageEmptyURL);
    await netErrorLoaded;

    await SpecialPowers.spawn(browser, [], () => {
      ok(
        content.document.documentURI.startsWith("about:neterror?e=netOffline"),
        "Should be showing error page"
      );
    });
  } finally {
    // Re-enable the proxy so example.com is resolved to localhost, rather than
    // the actual example.com.
    SpecialPowers.pushPrefEnv({
      set: [
        ["network.proxy.type", proxyPrefValue],
        ["browser.cache.disk.enable", diskPrefValue],
        ["browser.cache.memory.enable", memoryPrefValue],
      ],
    });
  }
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
