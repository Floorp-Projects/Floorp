/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that capturing a sites's thumbnail, saving it and
 * retrieving it from the cache works.
 */
function* runTests() {
  // Create a tab that shows an error page.
  let tab = BrowserTestUtils.addTab(gBrowser, "http://127.0.0.1:1/");
  let browser = tab.linkedBrowser;
  yield browser.addEventListener("DOMContentLoaded", function() {
    PageThumbs.shouldStoreThumbnail(browser, (aResult) => {
      ok(!aResult, "we're not going to capture an error page");
      executeSoon(next);
    });
  }, {once: true});
}
