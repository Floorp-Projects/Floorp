/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function testLinkModulePreload() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    ok(true, "We did not crash.");
    await helper.closeDialog();
  }, "file_link_modulepreload.html");
});
