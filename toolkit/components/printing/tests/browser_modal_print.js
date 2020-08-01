function assertExpectedPrintPage(helper) {
  let printBrowser = helper.win.PrintEventHandler.sourceBrowser;
  is(
    printBrowser,
    gBrowser.selectedBrowser,
    "The current browser is being printed"
  );
  is(
    printBrowser.currentURI.spec,
    PrintHelper.defaultTestPageUrl,
    "The URL of the browser is the one we expect"
  );
}

add_task(async function testModalPrintDialog() {
  await PrintHelper.withTestPage(async helper => {
    helper.assertDialogHidden();

    await helper.startPrint();

    helper.assertDialogVisible();

    // Check that we're printing the right page.
    assertExpectedPrintPage(helper);

    // Close the dialog with Escape.
    await helper.withClosingFn(() => {
      EventUtils.synthesizeKey("VK_ESCAPE", {}, helper.win);
    });

    helper.assertDialogHidden();
  });
});

add_task(async function testPrintMultiple() {
  await PrintHelper.withTestPage(async helper => {
    helper.assertDialogHidden();

    // First print as usual.
    await helper.startPrint();
    helper.assertDialogVisible();
    assertExpectedPrintPage(helper);

    // Trigger the command a few more times, verify the overlay still exists.
    await helper.startPrint();
    helper.assertDialogVisible();
    await helper.startPrint();
    helper.assertDialogVisible();
    await helper.startPrint();
    helper.assertDialogVisible();

    // Verify it's still the correct page.
    assertExpectedPrintPage(helper);

    // Make sure we clean up, ideally this would be handled by the helper.
    await helper.closeDialog();
  });
});

add_task(async function testCancelButton() {
  await PrintHelper.withTestPage(async helper => {
    helper.assertDialogHidden();
    await helper.startPrint();
    helper.assertDialogVisible();

    let cancelButton = helper.doc.querySelector("button[name=cancel]");
    ok(cancelButton, "Got the cancel button");
    await helper.withClosingFn(() =>
      EventUtils.synthesizeMouseAtCenter(cancelButton, {}, helper.win)
    );

    helper.assertDialogHidden();
  });
});
