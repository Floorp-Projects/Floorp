/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that capturing a sites's thumbnail, saving it and
 * retrieving it from the cache works.
 */
add_task(async function thumbnails_capture() {
  // Create a tab with a red background.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "data:text/html,<body bgcolor=ff0000></body>",
    },
    async browser => {
      await captureAndCheckColor(255, 0, 0, "we have a red thumbnail");

      // Load a page with a green background.
      let loaded = BrowserTestUtils.browserLoaded(browser);
      BrowserTestUtils.startLoadingURIString(
        browser,
        "data:text/html,<body bgcolor=00ff00></body>"
      );
      await loaded;
      await captureAndCheckColor(0, 255, 0, "we have a green thumbnail");

      // Load a page with a blue background.
      loaded = BrowserTestUtils.browserLoaded(browser);
      BrowserTestUtils.startLoadingURIString(
        browser,
        "data:text/html,<body bgcolor=0000ff></body>"
      );
      await loaded;
      await captureAndCheckColor(0, 0, 255, "we have a blue thumbnail");
    }
  );
});
