/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testModalPrintDialog() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    helper.assertDialogOpen();

    await helper.setupMockPrint();

    helper.doc.querySelector("#open-dialog-link").click();

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
    await helper.startPrint();

    helper.assertDialogOpen();

    await helper.setupMockPrint();

    helper.doc.querySelector("#open-dialog-link").click();

    helper.assertDialogHidden();

    await helper.withClosingFn(() => {
      helper.rejectShowSystemDialog();
    });

    helper.assertDialogClosed();
  });
});
