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

    await BrowserTestUtils.waitForCondition(
      () => !!document.querySelector(".printPreviewBrowser")
    );

    let previewBrowser = document.querySelector(
      ".printPreviewBrowser[previewtype='primary']"
    );
    let helper = new PrintHelper(browser);

    let textContent = await TestUtils.waitForCondition(() =>
      SpecialPowers.spawn(previewBrowser, [], function() {
        return content.document.body.textContent;
      })
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
