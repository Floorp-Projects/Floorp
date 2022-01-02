/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that capturing a sites's thumbnail, saving it and
 * retrieving it from the cache works, specifically for a parent process.
 */
add_task(async function thumbnails_capture() {
  // Create a parent process tab with a red background.
  // We do this by creating a parent process, then we update it to be a red page,
  // before attempting to read the page colour.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      // about:robots seems to be an simple parent process url we can test against,
      // but any parent process url would have worked, example, about:home or about:config
      url: "about:robots",
    },
    async browser => {
      // Because about:robots is not generally a predictable testable page,
      // we update its background to something we can test against.
      browser.contentDocument.body.innerHTML = "";
      browser.contentDocument.body.style.backgroundColor = "#ff0000";
      await captureAndCheckColor(255, 0, 0, "we have a red thumbnail");
    }
  );
});
