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
add_task(async function testTabModal() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    ok(true, "We did not crash.");
    await helper.closeDialog();
  }, "file_coop_header.html");
});
