"use strict";

const SAMESITE_DOMAIN = "http://example.com/";
const SAMESITE_PATH = "browser/netwerk/cookie/test/browser/";
const SAMESITE_TOP_PAGE = SAMESITE_DOMAIN + SAMESITE_PATH + "sameSite.sjs";

add_task(async _ => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.sameSite.laxByDefault", true],
      ["network.cookie.sameSite.noneRequiresSecure", true],
    ],
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
          "Cookie “a” has “sameSite” policy set to “lax” because it is missing a “sameSite” attribute, and “sameSite=lax” is the default value for this attribute.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “b” rejected because it has the “sameSite=none” attribute but is missing the “secure” attribute.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Invalid “sameSite“ value for cookie “c”. The supported values are: “lax“, “strict“, “none“.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “c” has “sameSite” policy set to “lax” because it is missing a “sameSite” attribute, and “sameSite=lax” is the default value for this attribute.",
      });
    }),
  ];

  // Let's open our tab.
  const tab = BrowserTestUtils.addTab(gBrowser, SAMESITE_TOP_PAGE);
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
          "Cookie “d” has “sameSite” policy set to “lax” because it is missing a “sameSite” attribute, and “sameSite=lax” is the default value for this attribute.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “e” rejected because it has the “sameSite=none” attribute but is missing the “secure” attribute.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Invalid “sameSite“ value for cookie “f”. The supported values are: “lax“, “strict“, “none“.",
      });
    }),

    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie “f” has “sameSite” policy set to “lax” because it is missing a “sameSite” attribute, and “sameSite=lax” is the default value for this attribute.",
      });
    }),
  ];

  // Let's use document.cookie
  SpecialPowers.spawn(browser, [], () => {
    content.document.cookie = "d=4";
    content.document.cookie = "e=5; sameSite=none";
    content.document.cookie = "f=6; sameSite=batmat";
  });

  // Let's wait for the dom events.
  await Promise.all(domPromises);

  // Let's close the tab.
  BrowserTestUtils.removeTab(tab);
});
