/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/" +
            "test/background_red_redirect.sjs";
// loading URL will redirect us to...
const FINAL_URL = "http://mochi.test:8888/browser/toolkit/components/" +
                  "thumbnails/test/background_red.html";

/**
 * These tests ensure that we save and provide thumbnails for redirecting sites.
 */
function runTests() {
  dontExpireThumbnailURLs([URL, FINAL_URL]);

  // Kick off history by loading a tab first or the test fails in single mode.
  yield addTab(URL);
  gBrowser.removeTab(gBrowser.selectedTab);

  // Create a tab, redirecting to a page with a red background.
  yield addTab(URL);
  yield captureAndCheckColor(255, 0, 0, "we have a red thumbnail");

  // Wait until the referrer's thumbnail's file has been written.
  yield whenFileExists(URL);
  yield retrieveImageDataForURL(URL, function ([r, g, b]) {
    is("" + [r,g,b], "255,0,0", "referrer has a red thumbnail");
    next();
  });
}
