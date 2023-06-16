/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

// This is a modified version from browser_contextmenuFillLogins.js.
async function openContextMenuForSelector(browser, selector) {
  const doc = browser.ownerDocument;
  const CONTEXT_MENU = doc.getElementById("contentAreaContextMenu");

  let contextMenuShownPromise = BrowserTestUtils.waitForEvent(
    CONTEXT_MENU,
    "popupshown"
  );

  let inputCoords = await SpecialPowers.spawn(
    browser,
    [selector],
    async selector => {
      let input = content.document.querySelector(selector);
      input.focus();
      let inputRect = input.getBoundingClientRect();

      // listen for the contextmenu event so we can assert on receiving it
      // and examine the target
      content.contextmenuPromise = new Promise(resolve => {
        content.document.body.addEventListener(
          "contextmenu",
          event => {
            info(
              `Received event on target: ${event.target.nodeName}, type: ${event.target.type}`
            );
            content.console.log("got contextmenu event: ", event);
            resolve(event);
          },
          { once: true }
        );
      });

      let coords = {
        x: inputRect.x + inputRect.width / 2,
        y: inputRect.y + inputRect.height / 2,
      };
      return coords;
    }
  );

  // add the offsets of the <browser> in the chrome window
  let browserOffsets = browser.getBoundingClientRect();
  let offsetX = browserOffsets.x + inputCoords.x;
  let offsetY = browserOffsets.y + inputCoords.y;

  // Synthesize a right mouse click over the input element, we have to trigger
  // both events because formfill code relies on this event happening before the contextmenu
  // (which it does for real user input) in order to not show the password autocomplete.
  let eventDetails = { type: "mousedown", button: 2 };
  await EventUtils.synthesizeMouseAtPoint(offsetX, offsetY, eventDetails);

  // Synthesize a contextmenu event to actually open the context menu.
  eventDetails = { type: "contextmenu", button: 2 };
  await EventUtils.synthesizeMouseAtPoint(offsetX, offsetY, eventDetails);

  await SpecialPowers.spawn(browser, [], async () => {
    await content.contextmenuPromise;
  });

  info("waiting for contextMenuShownPromise");

  await contextMenuShownPromise;
  return CONTEXT_MENU;
}

/**
 * Ensure the fill login context menu is not shown for PDF.js forms.
 */
add_task(async function test_filllogin() {
  await SpecialPowers.pushPrefEnv({
    set: [["pdfjs.renderInteractiveForms", true]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_pdfjs_form.pdf"
      );

      let contextMenu = await openContextMenuForSelector(
        browser,
        "#viewerContainer input"
      );
      let fillItem = contextMenu.querySelector("#fill-login");
      ok(fillItem, "fill menu item exists");
      ok(fillItem && EventUtils.isHidden(fillItem), "fill menu item is hidden");

      let promiseHidden = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      info("Calling hidePopup on contextMenu");
      contextMenu.hidePopup();
      info("waiting for promiseHidden");
      await promiseHidden;
    }
  );
});
