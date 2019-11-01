/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL =
  "http://mochi.test:8888/browser/toolkit/components/thumbnails/" +
  "test/background_red_redirect.sjs";
// loading URL will redirect us to...
const FINAL_URL =
  "http://mochi.test:8888/browser/toolkit/components/" +
  "thumbnails/test/background_red.html";

/**
 * These tests ensure that we save and provide thumbnails for redirecting sites.
 */
add_task(async function thumbnails_redirect() {
  dontExpireThumbnailURLs([URL, FINAL_URL]);

  // Kick off history by loading a tab first or the test fails in single mode.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    browser => {}
  );

  // Create a tab, redirecting to a page with a red background.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      await captureAndCheckColor(255, 0, 0, "we have a red thumbnail");

      // Wait until the referrer's thumbnail's file has been written.
      await whenFileExists(URL);
      let [r, g, b] = await retrieveImageDataForURL(URL);
      is("" + [r, g, b], "255,0,0", "referrer has a red thumbnail");
    }
  );
});
