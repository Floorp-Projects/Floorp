/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  let tab = await BrowserTestUtils.switchTab(gBrowser, function() {
    gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      `${TEST_PATH}simplifyArticleSample.html`,
      { userContextId: 1 }
    );
  });

  const helper = new PrintHelper(tab.linkedBrowser);

  helper.assertDialogHidden();

  await helper.startPrint();

  helper.assertDialogVisible();

  let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
  file.append("browser_print_in_container.pdf");
  ok(!file.exists(), "The file doesn't exist yet");

  helper.mockFilePicker(file);
  registerCleanupFunction(() => {
    if (file.exists()) {
      file.remove(false);
    }
  });

  await helper.withClosingFn(() => {
    helper.click(helper.get("print-button"));
  });

  await TestUtils.waitForCondition(
    () => file.exists(),
    "Wait for printed file"
  );
  ok(file.exists(), "The file was printed");

  ok(true, "We did not crash.");

  BrowserTestUtils.removeTab(tab);
});
