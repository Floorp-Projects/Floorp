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

  await BrowserTestUtils.withNewTab(
    `${TEST_PATH}file_window_print.html`,
    async function(browser) {
      info(
        "Waiting for the first window.print() to run and ensure we're showing the preview..."
      );

      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

      {
        let [before, afterFirst] = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return [
              !!content.document.getElementById("before-print"),
              !!content.document.getElementById("after-first-print"),
            ];
          }
        );

        ok(before, "Content before printing should be in the DOM");
        ok(!afterFirst, "Shouldn't have returned yet from window.print()");
      }

      gBrowser.getTabDialogBox(browser).abortAllDialogs();

      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

      {
        let [before, afterFirst, afterSecond] = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return [
              !!content.document.getElementById("before-print"),
              !!content.document.getElementById("after-first-print"),
              !!content.document.getElementById("after-second-print"),
            ];
          }
        );

        ok(before, "Content before printing should be in the DOM");
        ok(afterFirst, "Should be in the second print already");
        ok(afterSecond, "Shouldn't have blocked if we have mozPrintCallbacks");
      }
    }
  );
});
