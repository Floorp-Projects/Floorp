/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
'use strict';

// Check if the browser has loaded by checking ready state of the DOM.
function isDOMLoaded(aBrowser) {
  return aBrowser.contentWindowAsCPOW.document.readyState === 'complete';
}

// It checks if calling BrowserTestUtils.browserLoaded() yields
// browser object.
add_task(function*() {
  let tab = gBrowser.addTab('http://example.com');
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);
  Assert.ok(isDOMLoaded(browser), 'browser', 'Expect browser to have loaded.');
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
    for (u of tabURLs) gBrowser.addTab(u).linkedBrowser
  ];
  //wait for promises to settle
  yield Promise.all((
    for (b of browsers) BrowserTestUtils.browserLoaded(b)
  ));
  let expected = 'Expected all promised browsers to have loaded.';
  Assert.ok(browsers.every(isDOMLoaded), expected);
  //cleanup
  browsers
    .map(browser => gBrowser.getTabForBrowser(browser))
    .forEach(tab => gBrowser.removeTab(tab));
});
