/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_PROTECTION_REPORT() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser);

    await SMATestUtils.executeAndValidateAction({
      type: "OPEN_PROTECTION_REPORT",
    });

    Assert.equal(
      await loaded,
      "about:protections",
      "should load about:protections"
    );
  });
});
