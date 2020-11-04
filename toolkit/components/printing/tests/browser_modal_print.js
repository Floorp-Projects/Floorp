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

    const lastButtonName = AppConstants.platform == "win" ? "cancel" : "print";
    const lastButton = helper.doc.querySelector(
      `button[name=${lastButtonName}]`
    );
    focused = BrowserTestUtils.waitForEvent(lastButton, "focus");
    lastButton.focus();
    await focused;
    ok(true, "Last button focused");

    focused = BrowserTestUtils.waitForEvent(gNavToolbox, "focus", true);
    EventUtils.synthesizeKey("KEY_Tab");
    await focused;
    ok(true, "Toolbox focused after tab");

    focused = BrowserTestUtils.waitForEvent(lastButton, "focus");
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    await focused;
    ok(true, "Cancel button focused after shift+tab");

    await helper.withClosingFn(() => {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });

    helper.assertDialogClosed();
  });
});

async function testPrintWithEnter(testFn, filename) {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    let file = helper.mockFilePicker(filename);
    await testFn(helper);
    await helper.assertPrintToFile(file, () => {
      EventUtils.sendKey("return", helper.win);
    });
  });
}

add_task(async function testEnterAfterLoadPrints() {
  info("Test print without moving focus");
  await testPrintWithEnter(() => {}, "print_initial_focus.pdf");
});

add_task(async function testEnterPrintsFromPageRangeSelect() {
  info("Test print from page range select");
  await testPrintWithEnter(helper => {
    let pageRangePicker = helper.get("range-picker");
    pageRangePicker.focus();
    is(
      helper.doc.activeElement,
      pageRangePicker,
      "Page range select is focused"
    );
  }, "print_page_range_select.pdf");
});

add_task(async function testEnterPrintsFromOrientation() {
  info("Test print on Enter from focused orientation input");
  await testPrintWithEnter(helper => {
    let portrait = helper.get("portrait");
    portrait.focus();
    is(helper.doc.activeElement, portrait, "Portrait is focused");
  }, "print_orientation_focused.pdf");
});

add_task(async function testPrintOnNewWindowDoesntClose() {
  info("Test that printing doesn't close a window with a single tab");
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let browser = win.gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, PrintHelper.defaultTestPageUrl);
  await BrowserTestUtils.browserLoaded(
    browser,
    true,
    PrintHelper.defaultTestPageUrl
  );
  let helper = new PrintHelper(browser);
  await helper.startPrint();
  let file = helper.mockFilePicker("print_new_window_close.pdf");
  await helper.assertPrintToFile(file, () => {
    EventUtils.sendKey("return", helper.win);
  });
  ok(!win.closed, "Shouldn't be closed");
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});
