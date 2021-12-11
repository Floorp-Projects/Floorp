/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  const FILE_PAGE = Services.io.newFileURI(
    new FileUtils.File(getTestFilePath("dummy.html"))
  ).spec;
  await BrowserTestUtils.withNewTab(FILE_PAGE, async browser => {
    try {
      await SpecialPowers.spawn(browser, [], () =>
        content.fetch("./file_lnk.lnk")
      );
      ok(
        false,
        "Loading lnk must fail if it links to a file from other directory"
      );
    } catch (err) {
      is(err.constructor.name, "TypeError", "Should fail on Windows");
    }
  });
});
