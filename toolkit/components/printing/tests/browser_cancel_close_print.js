/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testCloseWhilePrinting() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.setupMockPrint();
    helper.mockFilePicker("output.pdf");

    await helper.withClosingFn(async () => {
      let cancelButton = helper.get("cancel-button");
      is(
        helper.doc.l10n.getAttributes(cancelButton).id,
        "printui-cancel-button",
        "The cancel button is using the 'cancel' string"
      );
      EventUtils.sendKey("return", helper.win);
      is(
        helper.doc.l10n.getAttributes(cancelButton).id,
        "printui-close-button",
        "The cancel button is using the 'close' string"
      );
      helper.resolvePrint();
    });
  });
});
