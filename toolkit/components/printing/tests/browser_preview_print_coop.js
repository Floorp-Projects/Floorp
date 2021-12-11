/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

/**
 * Verify if the page with a COOP header can be used for printing preview.
 */
add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", false]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `${TEST_PATH}file_coop_header.html`
  );

  // Enter print preview
  let ppBrowser = PrintPreviewListener.getPrintPreviewBrowser();
  let ppPromise = PrintHelper.waitForOldPrintPreview(ppBrowser);
  document.getElementById("cmd_printPreview").doCommand();
  await ppPromise;

  await BrowserTestUtils.waitForCondition(
    () => gInPrintPreviewMode,
    "Should be in print preview mode now."
  );

  ok(true, "We did not crash.");

  PrintUtils.exitPrintPreview();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testTabModal() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    ok(true, "We did not crash.");
    await helper.closeDialog();
  }, "file_coop_header.html");
});
