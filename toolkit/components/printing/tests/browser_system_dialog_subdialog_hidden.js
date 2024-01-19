/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSystemDialogLinkState() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    is(
      helper.get("printer-picker").options.length,
      1,
      "Only the Save to PDF printer is available"
    );

    let systemLink = helper.get("open-dialog-link");
    if (AppConstants.platform == "win") {
      ok(
        BrowserTestUtils.isHidden(systemLink),
        "Link is hidden on Windows with no extra printers"
      );
    } else {
      ok(
        BrowserTestUtils.isVisible(systemLink),
        "Link is visible on Linux/macOS"
      );
    }
  });
});

add_task(async function testModalPrintDialog() {
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter("A printer");
    await SpecialPowers.pushPrefEnv({
      set: [["print_printer", "A printer"]],
    });

    await helper.startPrint();

    helper.assertDialogOpen();

    helper.assertSettingsMatch({ printerName: "A printer" });
    await helper.setupMockPrint();

    helper.click(helper.get("open-dialog-link"));

    helper.assertDialogHidden();

    await helper.withClosingFn(() => {
      helper.resolveShowSystemDialog();
      helper.resolvePrint();
    });

    helper.assertDialogClosed();
  });
});

add_task(async function testModalPrintDialogCancelled() {
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter("A printer");
    await SpecialPowers.pushPrefEnv({
      set: [["print_printer", "A printer"]],
    });

    await helper.startPrint();

    helper.assertDialogOpen();

    helper.assertSettingsMatch({ printerName: "A printer" });
    await helper.setupMockPrint();

    helper.click(helper.get("open-dialog-link"));

    helper.assertDialogHidden();

    await helper.withClosingFn(() => {
      helper.resolveShowSystemDialog(false);
    });

    helper.assertDialogClosed();
  });
});

add_task(async function testPrintDoesNotWaitForPreview() {
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter("A printer");
    await SpecialPowers.pushPrefEnv({
      set: [["print_printer", "A printer"]],
    });

    await helper.startPrint({ waitFor: "loadComplete" });
    await helper.awaitAnimationFrame();

    helper.mockFilePicker("print_does_not_wait_for_preview.pdf");
    await helper.setupMockPrint();

    let systemPrint = helper.get("system-print");
    await BrowserTestUtils.waitForCondition(
      () => BrowserTestUtils.isVisible(systemPrint),
      "Wait for the system-print to be visible"
    );

    helper.click(helper.get("open-dialog-link"));

    helper.assertDialogHidden();
    await helper.withClosingFn(() => {
      helper.resolveShowSystemDialog();
      helper.resolvePrint();
    });

    helper.assertDialogClosed();
  });
});
