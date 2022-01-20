/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const frameSource = `<a href="about:mozilla">Inner frame</a>`;
const source = `<html><h1>Top level text</h1><iframe srcdoc='${frameSource}' id="f"></iframe></html>`;

add_task(async function testPrintFrame() {
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  let url = `data:text/html,${source}`;
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function(browser) {
    let contentAreaContextMenuPopup = document.getElementById(
      "contentAreaContextMenu"
    );
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      contentAreaContextMenuPopup,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#f",
      { type: "contextmenu", button: 2 },
      gBrowser.selectedBrowser
    );
    await popupShownPromise;

    let frameItem = document.getElementById("frame");
    let frameContextMenu = frameItem.menupopup;
    popupShownPromise = BrowserTestUtils.waitForEvent(
      frameContextMenu,
      "popupshown"
    );
    frameItem.openMenu(true);
    await popupShownPromise;

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      frameContextMenu,
      "popuphidden"
    );
    let item = document.getElementById("context-printframe");
    frameContextMenu.activateItem(item);
    await popupHiddenPromise;

    let helper = new PrintHelper(browser);

    await helper.waitForDialog();

    let previewBrowser = helper.currentPrintPreviewBrowser;
    is(
      previewBrowser.getAttribute("previewtype"),
      "source",
      "Source preview was rendered"
    );

    let textContent = await SpecialPowers.spawn(
      previewBrowser,
      [],
      () => content.document.body.textContent
    );

    is(textContent, "Inner frame", "Correct content loaded");
    is(
      helper.win.PrintEventHandler.printFrameOnly,
      true,
      "Print frame only is true"
    );
    PrintHelper.resetPrintPrefs();
  });
});
