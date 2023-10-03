/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["print.enabled", false]],
  });
});

add_task(async function test_print_commands_disabled() {
  let printCommand = document.getElementById(`cmd_print`);
  let printPreview = document.getElementById(`cmd_printPreviewToggle`);

  is(
    printCommand.getAttribute("disabled"),
    "true",
    "print command is disabled when print.enabled=false"
  );
  is(
    printPreview.getAttribute("disabled"),
    "true",
    "print preview command is disabled when print.enabled=false"
  );
});

add_task(async function test_window_print_disabled() {
  await BrowserTestUtils.withNewTab("https://example.org/", async browser => {
    await SpecialPowers.spawn(browser, [], function () {
      content.window.print();
    });

    // When the print dialog is open, the main thread of our tab is blocked, so
    // the failure mode of this test is a timeout caused by the print dialog never
    // closing.

    Assert.ok(
      true,
      "window.print doesn't do anything when print.enabled=false"
    );
  });
});
