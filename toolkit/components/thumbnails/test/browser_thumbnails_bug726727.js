/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that capturing a sites's thumbnail, saving it and
 * retrieving it from the cache works.
 */
add_task(async function thumbnails_bg_bug726727() {
  // Create a tab that shows an error page.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
    },
    async browser => {
      let errorPageLoaded = BrowserTestUtils.browserLoaded(
        browser,
        false,
        null,
        true
      );
      BrowserTestUtils.startLoadingURIString(browser, "http://127.0.0.1:1");
      await errorPageLoaded;
      let result = await PageThumbs.shouldStoreThumbnail(browser);
      ok(!result, "we're not going to capture an error page");
    }
  );
});
