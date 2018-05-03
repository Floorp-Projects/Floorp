/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function isDOMLoaded(browser) {
  return ContentTask.spawn(browser, null, async function() {
    Assert.equal(content.document.readyState, "complete",
      "Browser should be loaded.");
  });
}

// It checks if calling BrowserTestUtils.browserLoaded() yields
// browser object.
add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com");
  let browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);
  await isDOMLoaded(browser);
  gBrowser.removeTab(tab);
});

// It checks that BrowserTestUtils.browserLoaded() works well with
// promise.all().
add_task(async function() {
  let tabURLs = [
    `http://example.org`,
    `http://mochi.test:8888`,
    `http://test:80`,
  ];
  // Add tabs, get the respective browsers
  let browsers = tabURLs.map(u => BrowserTestUtils.addTab(gBrowser, u).linkedBrowser);

  // wait for promises to settle
  await Promise.all((function* () {
    for (let b of browsers) {
      yield BrowserTestUtils.browserLoaded(b);
    }
  })());
  for (const browser of browsers) {
    await isDOMLoaded(browser);
  }
  // cleanup
  browsers
    .map(browser => gBrowser.getTabForBrowser(browser))
    .forEach(tab => gBrowser.removeTab(tab));
});
