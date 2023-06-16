/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test() {
  let tab = await BrowserTestUtils.switchTab(gBrowser, function () {
    gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      `${TEST_PATH}simplifyArticleSample.html`,
      { userContextId: 1 }
    );
  });

  const helper = new PrintHelper(tab.linkedBrowser);

  helper.assertDialogClosed();
  await helper.startPrint();
  helper.assertDialogOpen();

  let file = helper.mockFilePicker("browser_print_in_container.pdf");
  await helper.assertPrintToFile(file, () => {
    helper.click(helper.get("print-button"));
  });

  ok(true, "We did not crash.");

  BrowserTestUtils.removeTab(tab);
});
