/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testToggleToolbarButton() {
  await PrintHelper.withTestPage(async helper => {
    CustomizableUI.addWidgetToArea("print-button", CustomizableUI.AREA_NAVBAR);

    helper.assertDialogClosed();

    // get the button from the toolbar
    let button = document.getElementById("print-button");
    // click the toolbar button
    EventUtils.synthesizeMouseAtCenter(button, {});

    await helper.waitForDialog();

    // ensure dialog box is open
    helper.assertDialogOpen();

    // click toolbar button again to close dialog box
    EventUtils.synthesizeMouseAtCenter(button, {});

    helper.assertDialogClosed();
  });
});
