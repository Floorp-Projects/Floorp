/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function assertExpectedPrintPage(helper) {
  is(
    helper.sourceURI,
    PrintHelper.defaultTestPageUrlHTTPS,
    "The URL of the browser is the one we expect"
  );
}

add_task(async function testModalPrintDialog() {
  await PrintHelper.withTestPageHTTPS(async helper => {
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
  await PrintHelper.withTestPageHTTPS(async helper => {
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
    await TestUtils.waitForTick();
    await helper.closeDialog();
  });
});

add_task(async function testCancelButton() {
  await PrintHelper.withTestPageHTTPS(async helper => {
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
  await PrintHelper.withTestPageHTTPS(async helper => {
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

    let focused;
    let navigationShadowRoot = document.querySelector(".printPreviewNavigation")
      .shadowRoot;
    for (let buttonId of [
      "navigateEnd",
      "navigateNext",
      "navigatePrevious",
      "navigateHome",
    ]) {
      let button = navigationShadowRoot.getElementById(buttonId);
      focused = BrowserTestUtils.waitForEvent(button, "focus");
      await EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
      await focused;
    }

    focused = BrowserTestUtils.waitForEvent(previewBrowser, "focus");
    await EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    await focused;
    ok(true, "Print preview focused after shift+tab through the paginator");

    focused = BrowserTestUtils.waitForEvent(gNavToolbox, "focus", true);
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    await focused;
    ok(true, "Toolbox focused after shift+tab");

    focused = BrowserTestUtils.waitForEvent(previewBrowser, "focus");
    EventUtils.synthesizeKey("KEY_Tab");
    await focused;
    ok(true, "Print preview focused after tab");

    for (let buttonId of [
      "navigateHome",
      "navigatePrevious",
      "navigateNext",
      "navigateEnd",
    ]) {
      let button = navigationShadowRoot.getElementById(buttonId);
      focused = BrowserTestUtils.waitForEvent(button, "focus");
      await EventUtils.synthesizeKey("KEY_Tab");
      await focused;
    }
    focused = BrowserTestUtils.waitForEvent(printerPicker, "focus");
    EventUtils.synthesizeKey("KEY_Tab");
    await focused;
    ok(true, "Printer picker focused after tab");

    const lastButton = helper.doc.querySelector(
      `#button-container > button:last-child`
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
    ok(true, "Last button focused after shift+tab");

    await helper.withClosingFn(() => {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });

    helper.assertDialogClosed();
  });
});

async function testPrintWithEnter(testFn, filename) {
  await PrintHelper.withTestPageHTTPS(async helper => {
    await helper.startPrint();

    let file = helper.mockFilePicker(filename);
    await testFn(helper);
    await helper.assertPrintToFile(file, () => {
      EventUtils.sendKey("return", helper.win);
      const cancelButton = helper.doc.querySelector(`button[name="cancel"]`);
      ok(!cancelButton.disabled, "Cancel button is not disabled");
      const printButton = helper.doc.querySelector(`button[name="print"]`);
      ok(printButton.disabled, "Print button is disabled");
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
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await PrintHelper.withTestPageHTTPS(async helper => {
    await helper.startPrint();
    let file = helper.mockFilePicker("print_new_window_close.pdf");
    await helper.assertPrintToFile(file, () => {
      EventUtils.sendKey("return", helper.win);
    });
  });
  ok(!win.closed, "Shouldn't be closed");
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function testPrintProgressIndicator() {
  await PrintHelper.withTestPageHTTPS(async helper => {
    await helper.startPrint();

    helper.setupMockPrint();

    let progressIndicator = helper.get("print-progress");
    ok(progressIndicator.hidden, "Progress indicator is hidden");

    let indicatorShown = BrowserTestUtils.waitForAttributeRemoval(
      "hidden",
      progressIndicator
    );
    helper.click(helper.get("print-button"));
    await indicatorShown;

    ok(!progressIndicator.hidden, "Progress indicator is shown on print start");

    await helper.withClosingFn(async () => {
      await helper.resolvePrint();
    });
  });
});

add_task(async function testPageSizePortrait() {
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.page-size.enabled", true]],
  });
  await PrintHelper.withTestPageHTTPS(async helper => {
    await helper.startPrint();

    let orientation = helper.get("orientation");
    ok(orientation.hidden, "Orientation selector is hidden");

    is(
      helper.settings.orientation,
      Ci.nsIPrintSettings.kPortraitOrientation,
      "Orientation set to portrait"
    );
  }, "file_portrait.html");
});

add_task(async function testPageSizeLandscape() {
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.page-size.enabled", true]],
  });
  await PrintHelper.withTestPageHTTPS(async helper => {
    await helper.startPrint();

    let orientation = helper.get("orientation");
    ok(orientation.hidden, "Orientation selector is hidden");

    is(
      helper.settings.orientation,
      Ci.nsIPrintSettings.kLandscapeOrientation,
      "Orientation set to landscape"
    );
  }, "file_landscape.html");
});
