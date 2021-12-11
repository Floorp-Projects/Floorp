"use strict";

const OVERSIZE_DOMAIN = "http://example.com/";
const OVERSIZE_PATH = "browser/netwerk/cookie/test/browser/";
const OVERSIZE_TOP_PAGE = OVERSIZE_DOMAIN + OVERSIZE_PATH + "oversize.sjs";

add_task(async _ => {
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
          "Cookie “a” is invalid because its size is too big. Max size is 4096 B.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “b” is invalid because its path size is too big. Max size is 1024 B.",
      });
    }),
  ];

  // Let's open our tab.
  const tab = BrowserTestUtils.addTab(gBrowser, OVERSIZE_TOP_PAGE);
  gBrowser.selectedTab = tab;

  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Let's wait for the first set of console events.
  await Promise.all(netPromises);

  // the DOM list of events.
  const domPromises = [
    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “d” is invalid because its size is too big. Max size is 4096 B.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “e” is invalid because its path size is too big. Max size is 1024 B.",
      });
    }),
  ];

  // Let's use document.cookie
  SpecialPowers.spawn(browser, [], () => {
    const maxBytesPerCookie = 4096;
    const maxBytesPerCookiePath = 1024;
    content.document.cookie = "d=" + Array(maxBytesPerCookie + 1).join("x");
    content.document.cookie =
      "e=f; path=/" + Array(maxBytesPerCookiePath + 1).join("x");
  });

  // Let's wait for the dom events.
  await Promise.all(domPromises);

  // Let's close the tab.
  BrowserTestUtils.removeTab(tab);
});
