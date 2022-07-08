/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

// This is a modified version from browser_contextmenuFillLogins.js.
async function openContextMenuAt(browser, x, y) {
  const doc = browser.ownerDocument;
  const contextMenu = doc.getElementById("contentAreaContextMenu");

  const contextMenuShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  // Synthesize a contextmenu event to actually open the context menu.
  await BrowserTestUtils.synthesizeMouseAtPoint(
    x,
    y,
    {
      type: "contextmenu",
      button: 2,
    },
    browser
  );

  await contextMenuShownPromise;
  return contextMenu;
}

/**
 * The text layer contains some spans with the text of the pdf.
 * @param {Object} browser
 * @param {string} text
 * @returns {Object} the bbox of the span containing the text.
 */
async function getSpanBox(browser, text) {
  return SpecialPowers.spawn(browser, [text], async function(text) {
    const { ContentTaskUtils } = ChromeUtils.import(
      "resource://testing-common/ContentTaskUtils.jsm"
    );
    const { document } = content;

    await ContentTaskUtils.waitForCondition(
      () => !!document.querySelector(".textLayer .endOfContent"),
      "The text layer must be displayed"
    );

    let targetSpan = null;
    for (const span of document.querySelectorAll(
      `.textLayer span[role="presentation"]`
    )) {
      if (span.innerText.includes(text)) {
        targetSpan = span;
        break;
      }
    }

    Assert.ok(targetSpan, `document must have a span containing '${text}'`);

    const { x, y, width, height } = targetSpan.getBoundingClientRect();
    return { x, y, width, height };
  });
}

/**
 * Open a context menu and get the pdfjs entries
 * @param {Object} browser
 * @param {Object} box
 * @returns {Map<string,HTMLElement>} the pdfjs menu entries.
 */
async function getContextMenuItems(browser, box) {
  const { x, y, width, height } = box;
  const menuitems = [
    "context-pdfjs-undo",
    "context-pdfjs-redo",
    "context-sep-pdfjs-redo",
    "context-pdfjs-cut",
    "context-pdfjs-copy",
    "context-pdfjs-paste",
    "context-pdfjs-delete",
    "context-pdfjs-selectall",
    "context-sep-pdfjs-selectall",
  ];

  await openContextMenuAt(browser, x + width / 2, y + height / 2);

  const results = new Map();
  const doc = browser.ownerDocument;
  for (const menuitem of menuitems) {
    const item = doc.getElementById(menuitem);
    results.set(menuitem, item || null);
  }

  return results;
}

/**
 * Open a context menu on the element corresponding to the given selector
 * and returs the pdfjs menu entries.
 * @param {Object} browser
 * @param {string} selector
 * @returns {Map<string,HTMLElement>} the pdfjs menu entries.
 */
async function getContextMenuItemsOn(browser, selector) {
  const box = await SpecialPowers.spawn(browser, [selector], async function(
    selector
  ) {
    const element = content.document.querySelector(selector);
    const { x, y, width, height } = element.getBoundingClientRect();
    return { x, y, width, height };
  });
  return getContextMenuItems(browser, box);
}

/**
 * Hide the context menu.
 * @param {Object} browser
 */
async function hideContextMenu(browser) {
  const doc = browser.ownerDocument;
  const contextMenu = doc.getElementById("contentAreaContextMenu");

  const popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
}

/**
 * Enable an editor (Ink, FreeText, ...).
 * @param {Object} browser
 * @param {string} name
 */
async function enableEditor(browser, name) {
  await SpecialPowers.spawn(browser, [name], async function(name) {
    const button = content.document.querySelector(`#editor${name}`);
    button.click();
  });
}

/**
 * Click at the given coordinates.
 * @param {Object} browser
 * @param {number} x
 * @param {number} y
 */
async function clickAt(browser, x, y) {
  BrowserTestUtils.synthesizeMouseAtPoint(
    x,
    y,
    {
      type: "mousedown",
      button: 0,
    },
    browser
  );
  BrowserTestUtils.synthesizeMouseAtPoint(
    x,
    y,
    {
      type: "mouseup",
      button: 0,
    },
    browser
  );
}

/**
 * Click on the element corresponding to the given selector.
 * @param {Object} browser
 * @param {string} selector
 */
async function clickOn(browser, selector) {
  const [x, y] = await SpecialPowers.spawn(browser, [selector], async function(
    selector
  ) {
    const element = content.document.querySelector(selector);
    const { x, y, width, height } = element.getBoundingClientRect();
    return [x + width / 2, y + height / 2];
  });
  await clickAt(browser, x, y);
}

/**
 * Write some text using the keyboard.
 * @param {Object} browser
 * @param {string} text
 */
async function write(browser, text) {
  await SpecialPowers.spawn(browser, [text], async function(text) {
    const { ContentTaskUtils } = ChromeUtils.import(
      "resource://testing-common/ContentTaskUtils.jsm"
    );
    const EventUtils = ContentTaskUtils.getEventUtils(content);

    for (const char of text.split("")) {
      EventUtils.synthesizeKey(char, {}, content);
    }
  });
}

/**
 * Add a FreeText annotation and write some text inside.
 * @param {Object} browser
 * @param {string} text
 * @param {Object} box
 */
async function addFreeText(browser, text, box) {
  const { x, y, width, height } = box;
  await clickAt(browser, x + 0.1 * width, y + 0.5 * height);
  await write(browser, text);
  await clickAt(browser, x + 0.1 * width, y + 2 * height);
}

/**
 * Count the number of elements corresponding to the given selector.
 * @param {Object} browser
 * @param {string} selector
 * @returns
 */
async function countElements(browser, selector) {
  return SpecialPowers.spawn(browser, [selector], async function(selector) {
    const { document } = content;
    return document.querySelectorAll(selector).length;
  });
}

/**
 * Asserts that the enabled pdfjs menuitems are the expected ones.
 * @param {Map<string,HTMLElement>} menuitems
 * @param {Array<string>} expected
 */
function assertMenuitems(menuitems, expected) {
  Assert.deepEqual(
    [...menuitems.values()]
      .filter(
        elmt =>
          !elmt.id.includes("-sep-") &&
          !elmt.hidden &&
          elmt.getAttribute("disabled") === "false"
      )
      .map(elmt => elmt.id),
    expected
  );
}

// Text copy, paste, undo, redo, delete and select all in using the context
// menu.
add_task(async function test() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  info("Pref action: " + handlerInfo.preferredAction);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      await SpecialPowers.pushPrefEnv({
        set: [["pdfjs.annotationEditorMode", 0]],
      });

      // check that PDF is opened with internal viewer
      await waitForPdfJS(browser, TESTROOT + "file_pdfjs_test.pdf");

      const spanBox = await getSpanBox(browser, "and found references");
      let menuitems = await getContextMenuItems(browser, spanBox);

      // Nothing have been edited, hence the context menu doesn't contain any
      // pdf entries.
      Assert.ok(
        [...menuitems.values()].every(elmt => elmt.hidden),
        "No visible pdf menuitem"
      );
      await hideContextMenu(browser);

      await enableEditor(browser, "FreeText");
      await addFreeText(browser, "hello", spanBox);

      Assert.equal(await countElements(browser, ".freeTextEditor"), 1);

      menuitems = await getContextMenuItems(browser, spanBox);
      assertMenuitems(menuitems, [
        "context-pdfjs-undo", // Last created editor is undoable
        "context-pdfjs-selectall", // and selectable.
      ]);
      // Undo.
      menuitems.get("context-pdfjs-undo").click();

      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "The FreeText editor must have been removed"
      );

      menuitems = await getContextMenuItems(browser, spanBox);

      // The editor removed thanks to "undo" is now redoable
      assertMenuitems(menuitems, ["context-pdfjs-redo"]);
      menuitems.get("context-pdfjs-redo").click();

      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        1,
        "The FreeText editor must have been added back"
      );

      await clickOn(browser, "#pdfjs_internal_editor_0");
      menuitems = await getContextMenuItemsOn(
        browser,
        "#pdfjs_internal_editor_0"
      );

      assertMenuitems(menuitems, [
        "context-pdfjs-undo",
        "context-pdfjs-cut",
        "context-pdfjs-copy",
        "context-pdfjs-delete",
        "context-pdfjs-selectall",
      ]);

      menuitems.get("context-pdfjs-cut").click();
      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "The FreeText editor must have been cut"
      );

      menuitems = await getContextMenuItems(browser, spanBox);
      assertMenuitems(menuitems, ["context-pdfjs-undo", "context-pdfjs-paste"]);

      menuitems.get("context-pdfjs-paste").click();
      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        1,
        "The FreeText editor must have been pasted"
      );

      await clickOn(browser, "#pdfjs_internal_editor_2");
      menuitems = await getContextMenuItemsOn(
        browser,
        "#pdfjs_internal_editor_2"
      );

      assertMenuitems(menuitems, [
        "context-pdfjs-undo",
        "context-pdfjs-cut",
        "context-pdfjs-copy",
        "context-pdfjs-paste",
        "context-pdfjs-delete",
        "context-pdfjs-selectall",
      ]);

      menuitems.get("context-pdfjs-delete").click();
      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "The FreeText editor must have been deleted"
      );

      menuitems = await getContextMenuItems(browser, spanBox);
      menuitems.get("context-pdfjs-paste").click();
      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        1,
        "The FreeText editor must have been pasted"
      );

      await clickOn(browser, "#pdfjs_internal_editor_3");
      menuitems = await getContextMenuItemsOn(
        browser,
        "#pdfjs_internal_editor_3"
      );
      menuitems.get("context-pdfjs-copy").click();
      menuitems.get("context-pdfjs-paste").click();
      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        2,
        "The FreeText editor must have been pasted"
      );

      menuitems = await getContextMenuItems(browser, spanBox);
      menuitems.get("context-pdfjs-selectall").click();
      menuitems.get("context-pdfjs-delete").click();
      await hideContextMenu(browser);

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "All the FreeText editors must have been deleted"
      );

      await SpecialPowers.spawn(browser, [], async function() {
        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
