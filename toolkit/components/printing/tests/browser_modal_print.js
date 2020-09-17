/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function assertExpectedPrintPage(helper) {
  is(
    helper.sourceURI,
    PrintHelper.defaultTestPageUrl,
    "The URL of the browser is the one we expect"
  );
}

add_task(async function testModalPrintDialog() {
  await PrintHelper.withTestPage(async helper => {
    helper.assertDialogClosed();

    await helper.startPrint();

    helper.assertDialogOpen();

    // Check that we're printing the right page.
    assertExpectedPrintPage(helper);

    // Close the dialog with Escape.
    await helper.withClosingFn(() => {
      EventUtils.synthesizeKey("VK_ESCAPE", {}, helper.win);
    });

    helper.assertDialogClosed();
  });
});

add_task(async function testPrintMultiple() {
  await PrintHelper.withTestPage(async helper => {
    helper.assertDialogClosed();

    // First print as usual.
    await helper.startPrint();
    helper.assertDialogOpen();
    assertExpectedPrintPage(helper);

    // Trigger the command a few more times, verify the overlay still exists.
    await helper.startPrint();
    helper.assertDialogOpen();
    await helper.startPrint();
    helper.assertDialogOpen();
    await helper.startPrint();
    helper.assertDialogOpen();

    // Verify it's still the correct page.
    assertExpectedPrintPage(helper);

    // Make sure we clean up, ideally this would be handled by the helper.
    await helper.closeDialog();
  });
});

add_task(async function testCancelButton() {
  await PrintHelper.withTestPage(async helper => {
    helper.assertDialogClosed();
    await helper.startPrint();
    helper.assertDialogOpen();

    let cancelButton = helper.doc.querySelector("button[name=cancel]");
    ok(cancelButton, "Got the cancel button");
    await helper.withClosingFn(() =>
      EventUtils.synthesizeMouseAtCenter(cancelButton, {}, helper.win)
    );

    helper.assertDialogClosed();
  });
});

add_task(async function testTabOrder() {
  await PrintHelper.withTestPage(async helper => {
    helper.assertDialogClosed();
    await helper.startPrint();
    helper.assertDialogOpen();

    const printerPicker = helper.doc.getElementById("printer-picker");
    is(
      helper.doc.activeElement,
      printerPicker,
      "Initial focus on printer picker"
    );

    const previewBrowser = document.querySelector(".printPreviewBrowser");
    ok(previewBrowser, "Got the print preview browser");
    let focused = BrowserTestUtils.waitForEvent(previewBrowser, "focus");
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    await focused;
    ok(true, "Print preview focused after shift+tab");

    focused = BrowserTestUtils.waitForEvent(gNavToolbox, "focus", true);
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    await focused;
    ok(true, "Toolbox focused after shift+tab");

    focused = BrowserTestUtils.waitForEvent(previewBrowser, "focus");
    EventUtils.synthesizeKey("KEY_Tab");
    await focused;
    ok(true, "Print preview focused after tab");

    focused = BrowserTestUtils.waitForEvent(printerPicker, "focus");
    EventUtils.synthesizeKey("KEY_Tab");
    await focused;
    ok(true, "Printer picker focused after tab");

    const cancelButton = helper.doc.querySelector("button[name=cancel]");
    ok(cancelButton, "Got the cancel button");
    focused = BrowserTestUtils.waitForEvent(cancelButton, "focus");
    cancelButton.focus();
    await focused;
    ok(true, "Cancel button focused");

    focused = BrowserTestUtils.waitForEvent(gNavToolbox, "focus", true);
    EventUtils.synthesizeKey("KEY_Tab");
    await focused;
    ok(true, "Toolbox focused after tab");

    focused = BrowserTestUtils.waitForEvent(cancelButton, "focus");
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    await focused;
    ok(true, "Cancel button focused after shift+tab");

    await helper.withClosingFn(() => {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });

    helper.assertDialogClosed();
  });
});
