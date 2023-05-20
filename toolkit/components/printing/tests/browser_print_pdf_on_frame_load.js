/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_print_pdf_on_frame_load() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_print_pdf_on_frame_load.html`,
    async function (browser) {
      info(
        "Waiting for window.print() to run and ensure we're showing the preview..."
      );

      let helper = new PrintHelper(browser);
      await waitForPreviewVisible();

      info("Preview shown, waiting for it to be updated...");

      await TestUtils.waitForCondition(() => helper.sheetCount != 0);

      is(helper.sheetCount, 2, "Both pages should be loaded");

      gBrowser.getTabDialogBox(browser).abortAllDialogs();
    }
  );
});
