/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_PROTECTION_REPORT() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      "about:protections"
    );

    await SMATestUtils.executeAndValidateAction({
      type: "OPEN_PROTECTION_REPORT",
    });

    await loaded;

    // When the graph is built it means any messaging has finished,
    // we can close the tab.
    await SpecialPowers.spawn(browser, [], async function() {
      await ContentTaskUtils.waitForCondition(() => {
        let bars = content.document.querySelectorAll(".graph-bar");
        return bars.length;
      }, "The graph has been built");
    });
  });
});
