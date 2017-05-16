/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
'use strict';

function isDOMLoaded(browser) {
  return ContentTask.spawn(browser, null, function*() {
    Assert.equal(content.document.readyState, "complete",
      "Browser should be loaded.");
  });
}

// It checks if calling BrowserTestUtils.browserLoaded() yields
// browser object.
add_task(function*() {
  let tab = BrowserTestUtils.addTab(gBrowser, 'http://example.com');
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);
  yield isDOMLoaded(browser);
  gBrowser.removeTab(tab);
});

// It checks that BrowserTestUtils.browserLoaded() works well with
// promise.all().
add_task(function*() {
  let tabURLs = [
    `http://example.org`,
    `http://mochi.test:8888`,
    `http://test:80`,
  ];
  //Add tabs, get the respective browsers
  let browsers = [
    for (u of tabURLs) BrowserTestUtils.addTab(gBrowser, u).linkedBrowser
  ];
  //wait for promises to settle
  yield Promise.all((
    for (b of browsers) BrowserTestUtils.browserLoaded(b)
  ));
  let expected = 'Expected all promised browsers to have loaded.';
  for (const browser of browsers) {
    yield isDOMLoaded(browser);
  }
  //cleanup
  browsers
    .map(browser => gBrowser.getTabForBrowser(browser))
    .forEach(tab => gBrowser.removeTab(tab));
});
