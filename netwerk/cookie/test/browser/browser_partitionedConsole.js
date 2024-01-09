"use strict";

const DOMAIN = "https://example.com/";
const PATH = "browser/netwerk/cookie/test/browser/";
const TOP_PAGE = DOMAIN + PATH + "file_empty.html";

// Run the test with CHIPS disabled, expecting a warning message
add_task(async _ => {
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior.optInPartitioning", false]],
  });

  const expected = [];

  const consoleListener = {
    observe(what) {
      if (!(what instanceof Ci.nsIConsoleMessage)) {
        return;
      }

      info("Console Listener: " + what);
      for (let i = expected.length - 1; i >= 0; --i) {
        const e = expected[i];

        if (what.message.includes(e.match)) {
          ok(true, "Message received: " + e.match);
          expected.splice(i, 1);
          e.resolve();
        }
      }
    },
  };

  Services.console.registerListener(consoleListener);

  registerCleanupFunction(() =>
    Services.console.unregisterListener(consoleListener)
  );

  const netPromises = [
    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “a” will soon be rejected because it is foreign and does not have the “Partitioned“ attribute.",
      });
    }),
  ];

  // Let's open our tab.
  const tab = BrowserTestUtils.addTab(gBrowser, TOP_PAGE);
  gBrowser.selectedTab = tab;
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Set cookies with cross-site HTTP
  await SpecialPowers.spawn(browser, [], async function () {
    await content.fetch(
      "https://example.org/browser/netwerk/cookie/test/browser/partitioned.sjs",
      { credentials: "include" }
    );
  });

  // Let's wait for the first set of console events.
  await Promise.all(netPromises);

  // Let's close the tab.
  BrowserTestUtils.removeTab(tab);
});

// Run the test with CHIPS enabled, expecting a different warning message
add_task(async _ => {
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior.optInPartitioning", true]],
  });

  const expected = [];

  const consoleListener = {
    observe(what) {
      if (!(what instanceof Ci.nsIConsoleMessage)) {
        return;
      }

      info("Console Listener: " + what);
      for (let i = expected.length - 1; i >= 0; --i) {
        const e = expected[i];

        if (what.message.includes(e.match)) {
          ok(true, "Message received: " + e.match);
          expected.splice(i, 1);
          e.resolve();
        }
      }
    },
  };

  Services.console.registerListener(consoleListener);

  registerCleanupFunction(() =>
    Services.console.unregisterListener(consoleListener)
  );

  const netPromises = [
    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “a” has been rejected because it is foreign and does not have the “Partitioned“ attribute.",
      });
    }),
  ];

  // Let's open our tab.
  const tab = BrowserTestUtils.addTab(gBrowser, TOP_PAGE);
  gBrowser.selectedTab = tab;
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Set cookies with cross-site HTTP
  await SpecialPowers.spawn(browser, [], async function () {
    await content.fetch(
      "https://example.org/browser/netwerk/cookie/test/browser/partitioned.sjs",
      { credentials: "include" }
    );
  });

  // Let's wait for the first set of console events.
  await Promise.all(netPromises);

  // Let's close the tab.
  BrowserTestUtils.removeTab(tab);
});
