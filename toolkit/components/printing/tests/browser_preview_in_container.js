/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test() {
  // window.print() only shows print preview when print.tab_modal.enabled is
  // true.
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  gBrowser.selectedTab = await BrowserTestUtils.addTab(
    gBrowser,
    `${TEST_PATH}file_print.html`,
    { userContextId: 1 }
  );

  // Wait for window.print() to run and ensure we're showing the preview...
  await BrowserTestUtils.waitForCondition(
    () => !!document.querySelector(".printPreviewBrowser")
  );

  let previewBrowser = document.querySelector(".printPreviewBrowser");
  let contentFound = await SpecialPowers.spawn(previewBrowser, [], () => {
    return !!content.document.getElementById("printed");
  });
  ok(contentFound, "We should find the preview content.");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
