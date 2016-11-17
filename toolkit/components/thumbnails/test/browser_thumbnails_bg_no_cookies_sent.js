/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* runTests() {
  // Visit the test page in the browser and tell it to set a cookie.
  let url = bgTestPageURL({ setGreenCookie: true });
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = tab.linkedBrowser;

  // The root element of the page shouldn't be green yet.
  yield ContentTask.spawn(browser, null, function() {
    Assert.notEqual(content.document.documentElement.style.backgroundColor,
                    "rgb(0, 255, 0)",
                    "The page shouldn't be green yet.");
  });

  // Cookie should be set now.  Reload the page to verify.  Its root element
  // will be green if the cookie's set.
  browser.reload();
  yield BrowserTestUtils.browserLoaded(browser);
  yield ContentTask.spawn(browser, null, function() {
    Assert.equal(content.document.documentElement.style.backgroundColor,
                 "rgb(0, 255, 0)",
                 "The page should be green now.");
  });

  // Capture the page.  Get the image data of the capture and verify it's not
  // green.  (Checking only the first pixel suffices.)
  yield bgCapture(url);
  ok(thumbnailExists(url), "Thumbnail file should exist after capture.");

  retrieveImageDataForURL(url, function([r, g, b]) {
    isnot([r, g, b].toString(), [0, 255, 0].toString(),
          "The captured page should not be green.");
    gBrowser.removeTab(tab);
    removeThumbnail(url);
    next();
  });
  yield true;
}
