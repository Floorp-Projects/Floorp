/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

async function runTest() {
  is(
    document.querySelector("print-preview"),
    null,
    "There shouldn't be any print preview browser"
  );

  gBrowser.selectedTab = await BrowserTestUtils.addTab(
    gBrowser,
    `${TEST_PATH}file_print.html`,
    { userContextId: 1 }
  );

  // Wait for window.print() to run and ensure we're showing the preview...
  await waitForPreviewVisible();

  let printPreviewEl = document.querySelector("print-preview");
  await BrowserTestUtils.waitForCondition(
    () => !!printPreviewEl.settingsBrowser.contentWindow._initialized
  );
  await printPreviewEl.settingsBrowser.contentWindow._initialized;
  let contentFound = await SpecialPowers.spawn(
    printPreviewEl.sourceBrowser,
    [],
    () => {
      return !!content.document.getElementById("printed");
    }
  );
  ok(contentFound, "We should find the preview content.");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function test_in_container() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.firstparty.isolate", false]],
  });

  await runTest();
});

add_task(async function test_with_fpi() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.firstparty.isolate", true]],
  });
  await runTest();
});
